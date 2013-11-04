/* otp_api.c */

/* 
  A single-purpose "HTTP server" that provides an OTP REST API for RRRR.
  It ignores everything but lines matching the pattern: GET *?querystring
  It converts querystring into an RRRR request, sends the request to the broker, and waits for a response.
  It then sends the response back to the HTTP client and closes the connection.
  It is event-driven (single-threaded, single-process) and multiplexes all TCP and ZMQ communication via a polling loop. 
*/

// $ time for i in {1..2000}; do curl localhost:9393/plan?0; done

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <czmq.h>
#include "util.h"
#include "config.h"
#include "router.h"
#include "parse.h"

// HTTP requires CR-LF style newlines. Headers are followed by two newlines.
#define CRLF          "\r\n"
#define HEADERS       CRLF
#define END_HEADERS   CRLF CRLF
#define TEXT_PLAIN    "Content-Type:text/plain"
#define APPLICATION_JSON    "Content-Type:application/json"
#define ALLOW_ORIGIN    "Access-Control-Allow-Origin:*"
#define ALLOW_HEADERS    "Access-Control-Allow-Headers:Requested-With,Content-Type"
#define OK_TEXT_PLAIN "HTTP/1.0 200 OK" HEADERS APPLICATION_JSON CRLF ALLOW_ORIGIN CRLF ALLOW_HEADERS CRLF
#define ERROR_404     "HTTP/1.0 404 Not Found" HEADERS TEXT_PLAIN END_HEADERS "FOUR ZERO FOUR" CRLF

#define BUFLEN     1024
#define PORT       9393 
#define QUEUE_CONN  500
#define MAX_CONN    100 // maximum number of simultaneous incoming HTTP connections

/* Buffers used to assemble and parse incoming HTTP requests. */
struct buffer {
    char  *buf;  // A pointer to the buffer, to allow efficient swapping
    int    size; // Number of bytes used in the buffer
    int    sd;   // Socket descriptor for this connection
//    time_t time; // Time at which last activity occurred, for timeouts
};

/* Poll items, including zmq broker, http listening, and http client communication sockets */
zmq_pollitem_t  poll_items [2 + MAX_CONN];

/* Open HTTP connections. */ 
zmq_pollitem_t *conn_items;        // Simply the tail of the poll_items array, without the first two items.
uint32_t        n_conn;            // Number of open connections.
struct buffer   buffers[MAX_CONN]; // We can swap these structs directly, including the char pointers they contain.

// A queue of connections to be removed at the end of the current polling iteration.
uint32_t conn_remove_queue[MAX_CONN];
uint32_t conn_remove_n = 0;

// Needed for parsing the query string.
tdata_t tdata;

// For looking up stops by location
HashGrid hash_grid;

/* 
  Schedule a connection for removal from the poll_items / open connections. It will be removed at the end of the 
  current polling iteration to avoid reordering other poll_items in the middle of an iteration.
  Note that scheduling the same connection for removal more than once will have unpredictable effects.
*/
static uint32_t remove_conn_later (uint32_t nc) {
    printf ("connection %02d [fd=%02d] enqueued for removal.\n", nc, conn_items[nc].fd);
    conn_remove_queue[conn_remove_n] = nc;
    conn_remove_n += 1;
    return conn_remove_n;
}

/* Debug function: print out all open connections. */
static void conn_dump_all () {
    printf ("number of active connections is %d\n", n_conn);
    for (int i = 0; i < n_conn; ++i) {
        zmq_pollitem_t *pi = poll_items + 2 + i;
        printf ("[%02d] fd=%02d buf='%s'\n", i, pi->fd, buffers[i].buf);
    }
}

/* Add a connection with socket descriptor sd to the end of the list of open connections. */
static void add_conn (uint32_t sd) {
    if (n_conn < MAX_CONN) {
        conn_items[n_conn].socket = NULL; // indicate that this is a standard socket, not a ZMQ socket
        conn_items[n_conn].fd = sd;
        conn_items[n_conn].events = POLLIN;
        printf ("connection %02d [fd=%02d] has been added.\n", n_conn, sd);
        n_conn++;
        conn_dump_all ();
    } else {
        // This should not happen since we suspend listen socket polling when the connection limit is reached.
        printf ("Accepted too many incoming connections, dropping one on the floor. \n");
    }
}

/* 
  Remove the HTTP connection with index nc from the list of open connections. 
  The last open connection in the list is swapped into the hole created.
  Returns true if the poll item was removed, false if the operation failed.
*/
static bool remove_conn (uint32_t nc) {
    if (nc >= n_conn) return false; // trying to remove an inactive connection
    uint32_t last_index = n_conn - 1;
    zmq_pollitem_t *item = conn_items + nc;
    zmq_pollitem_t *last = conn_items + last_index;
    printf ("connection %02d [fd=%02d] being removed.\n", nc, item->fd);
    memcpy (item, last, sizeof(*item));
    /* Swap in the buffer struct for the last active connection (retain char *buf). */
    struct buffer temp;
    temp = buffers[nc];
    buffers[nc] = buffers[last_index]; 
    buffers[last_index] = temp;
    buffers[last_index].size = 0;
    n_conn--;
    conn_dump_all ();
    return true;
}

// Removing a connection from pollitems and closing its SD are separate, because we have no connection number in the ZMQ response. Connection numbers are always changing.

/* Remove all connections that have been enqueued for removal in a single operation. */
static void remove_conn_enqueued () {
    for (int i = 0; i < conn_remove_n; ++i) {
        remove_conn (conn_remove_queue[i]);
    }
    conn_remove_n = 0;
}

/* 
  Read input from the socket associated with connection index nc into the corresponding buffer.
  Implementation note:
  POLLIN tells us that "data is available", which actually means "you can call read on this socket without blocking".
  If read/recv then returns 0 bytes, that indicates that the socket has been closed.
*/
static bool read_input (uint32_t nc) {
    struct buffer *b = &(buffers[nc]);
    int conn_sd = conn_items[nc].fd;
    char *c = b->buf + b->size; // pointer to the first available character in the buffer
    int remaining = BUFLEN - b->size;
    size_t received = recv (conn_sd, c, remaining, 0);
    printf ("connection %02d [fd=%02d] recevied %zd bytes.\n", nc, conn_sd, received);
    // If recv returns zero, that means the connection has been closed.
    // Don't remove it immediately, since we are in the middle of a poll loop.
    if (received == 0) {
        printf ("connection %02d [fd=%02d] closed. closing socket descriptor locally.\n", nc, conn_sd);
        remove_conn_later (nc);
        close (conn_sd); // client actively closed. necessary to close SD on server side as well.
        return false;
    }
    b->size += received;
    if (b->size >= BUFLEN) {
        printf ("HTTP request does not fit in buffer.\n");
        return false;
    }
    //printf ("received: %s \n", c);
    //printf ("buffer is now: %s \n", b->buf);
    bool eol = false;
    for (char *end = c + received; c <= end; ++c) {
        if (*c == '\n' || *c == '\r') {
            *c = '\0';
            eol = true;
            break;
        }
    }
    return eol;
}

static void send_request (int nc, void *broker_socket) {
    struct buffer *b = &(buffers[nc]);
    uint32_t conn_sd = conn_items[nc].fd;
    char *token = strtok (b->buf, " ");
    if (token == NULL) {
        printf ("request contained no verb \n");
        goto cleanup;
    }
    if (strcmp(token, "GET") != 0) {
        printf ("request was %s not GET \n", token);
        goto cleanup;
    }
    char *resource = strtok (NULL, " ");
    if (resource == NULL) {
        printf ("request contained no filename \n");
        goto cleanup;
    }
    char *qstring = index (resource, '?');
    if (qstring == NULL || qstring[1] == '\0') {
        printf ("request contained no query string \n");
        goto cleanup;
    }
    qstring += 1; // skip question mark
    router_request_t req;
    router_request_initialize (&req);
    router_request_randomize (&req, &tdata); // This prevents segfaults because data is not initialised
    parse_request_from_qstring(&req, &tdata, &hash_grid, qstring);
    zmsg_t *msg = zmsg_new ();
    zmsg_pushmem (msg, &req, sizeof(req));
    // Prefix the request with the socket descriptor for use upon reply. Worker ignores all frames but the last one.
    zmsg_pushmem (msg, &conn_sd, sizeof(conn_sd)); 
    zmsg_send (&msg, broker_socket);
    printf ("connection %02d [fd=%02d] sent request to broker.\n", nc, conn_sd);
    // TODO: Do not remove_conn_later yet. Continue polling so we detect client closing, 
    // avoiding TIME_WAIT on server side and supporting persistent connections.
    remove_conn_later (nc); // remove from poll list. stops accepting input from this connection.
    return;

    cleanup:
    send (conn_sd, ERROR_404, strlen(ERROR_404), MSG_NOSIGNAL);
    remove_conn_later (nc); // could this lead to double-remove?
    return;
}

void respond (int sd, char *response) {
    char buf[512];
    sprintf (buf, OK_TEXT_PLAIN "Content-Length: %zu" CRLF "Connection: close" END_HEADERS, strlen (response));
    // MSG_NOSIGNAL: Do not generate SIGPIPE if client has closed connection.
    // Send will return EPIPE if client already closed connection.
    send (sd, buf, strlen(buf), MSG_NOSIGNAL); 
    if (send (sd, response, strlen(response), MSG_NOSIGNAL) == -1 && errno == EPIPE) 
        printf ("              [fd=%02d] socket is closed, response dropped.\n", sd);
    else 
        printf ("              [fd=%02d] sent response to client.\n", sd);
}

int main (void) {
    
    tdata_load (RRRR_INPUT_FILE, &tdata);    
    coord_t coords[tdata.n_stops];
    for (uint32_t c = 0; c < tdata.n_stops; ++c) {
        coord_from_latlon(coords + c, tdata.stop_coords + c);
    }
    HashGrid_init (&hash_grid, 100, 500.0, coords, tdata.n_stops);
    
    /* Set up TCP/IP stream socket to listed for incoming HTTP requests. */
    struct sockaddr_in server_in_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY)
    };
    
    /* Listening socket is nonblocking: connections or bytes may not be waiting. */
    uint32_t server_socket = socket (AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    int one = 1; // Bind even when in TIME_WAIT due to a recently closed socket.
    setsockopt (server_socket, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    socklen_t in_addr_length = sizeof (server_in_addr);
    if (bind(server_socket, (struct sockaddr *) &server_in_addr, sizeof(server_in_addr)))
        die ("Failed to bind socket.\n");

    /* Check if we are root */
    if (getuid() == 0  || geteuid() == 0) {
        struct passwd *pw;
        uid_t puid = 65534; /* just use the traditional value */
        gid_t pgid = 65534;

        if ((pw = getpwnam("nobody"))) {
            puid = pw->pw_uid;
            pgid = pw->pw_gid;
        }

        /* Now we chroot to this directory, preventing any write access outside it */
        chroot("/var/empty");

        /* After we bind to the socket and chrooted, we drop priviledges to nobody */
        setuid(puid);
        setgid(pgid);
    }

    listen(server_socket, QUEUE_CONN);

    /* Set up ØMQ socket to communicate with the RRRR broker. */
    zctx_t *ctx = zctx_new ();
    void *broker_socket = zsocket_new (ctx, ZMQ_DEALER); // full async: dealer (api side) to router (broker side)
    if (zsocket_connect (broker_socket, CLIENT_ENDPOINT)) die ("RRRR OTP REST API server could not connect to broker.");
    
    /* Set up the poll_items for the main polling loop. */
    zmq_pollitem_t *broker_item = &(poll_items[0]);
    zmq_pollitem_t *http_item   = &(poll_items[1]);
    
    /* First poll item is ØMQ socket to and from the RRRR broker. */
    broker_item->socket = broker_socket;
    broker_item->fd = 0;
    broker_item->events = ZMQ_POLLIN;
    
    /* Second poll item is a standard socket for incoming HTTP requests. */
    http_item->socket = NULL; 
    http_item->fd = server_socket;
    http_item->events = ZMQ_POLLIN;
    
    /* The remaining poll_items are incoming HTTP connections. */
    conn_items  = &(poll_items[2]);
    n_conn = 0;
    
    /* Allocate buffers for incoming HTTP requests. */
    for (int i = 0; i < MAX_CONN; ++i) buffers[i].buf = malloc (BUFLEN);
    
    long n_in = 0, n_out = 0;
    for (;;) {
        /* Suspend polling (ignore enqueued incoming HTTP connections) when we already have too many. */
        http_item->events = n_conn < MAX_CONN ? ZMQ_POLLIN : 0;
        /* Blocking poll for queued incoming TCP connections, traffic on open TCP connections, and ZMQ broker events. */
        int n_waiting = zmq_poll (poll_items, 2 + n_conn, -1); 
        if (n_waiting < 1) {
            printf ("ZMQ poll call interrupted.\n"); 
            break; // Really, we should stop accepting incoming connections and break only when all connections closed.
        }
        /* Check if the ØMQ broker socket has a message for us. If so, write it out to the client socket. */
        if (broker_item->revents & ZMQ_POLLIN) {
            zmsg_t *msg = zmsg_recv (broker_socket);
            zframe_t *sd_frame = zmsg_pop (msg);
            uint32_t sd = *(zframe_data (sd_frame));
            char *response = zmsg_popstr (msg);
            // printf ("ZMQ broker socket received message for socket %02d:\n%s", sd, response);
            respond (sd, response);
            // We do not handle persistent connections. 
            // Connection has already been removed from polling list when ZMQ message was sent to broker.
            // Unfortunately if the ZMQ response never arrives, the SD will remain open indefinitely.
            close (sd); // server actively closes
            zmsg_destroy (&msg);
            n_waiting--;
        }
        /* Check if the listening TCP/IP socket has a queued connection. */
        if (http_item->revents & ZMQ_POLLIN) {
            struct sockaddr_in client_in_addr;
            socklen_t in_addr_length;
            // Adding a connection will increase the total connection count, but in the loop over open connections 
            // n_waiting should hit zero before the new one is encountered.
            // Checking open connections before adding the new one would be less efficient since each incoming 
            // connection would trigger an iteration through the whole list of (possibly inactive) existing connections.
            // Will these client sockets necessarily be nonblocking because the listening socket is?
            int client_socket = accept (server_socket, (struct sockaddr *) &client_in_addr, &in_addr_length);
            if (client_socket < 0) printf ("Error on TCP socket accept.\n");
            else add_conn (client_socket);
            n_waiting--;
        }
        /* Read from any open HTTP connections that have available input. */
        for (uint32_t c = 0; c < n_conn && n_waiting > 0; ++c) {
            if (conn_items[c].revents & ZMQ_POLLIN) {
                bool eol = read_input (c);
                n_waiting--;
                if (eol) send_request (c, broker_socket);
            }
        }
        /* Remove all connections found to be closed during this poll iteration. */
        remove_conn_enqueued (); 
    }
    zctx_destroy (&ctx);
    close (server_socket);
    return (0);
}

