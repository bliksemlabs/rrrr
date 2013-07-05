// Single-Threaded OTP API compatibility server

/* 
    $ time for i in {1..2000}; do curl localhost:9393/plan?0; done
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define OK_TEXT_PLAIN "HTTP/1.0 200 OK\nContent-Type:text/plain\n\n"
#define ERROR_404     "HTTP/1.0 404 Not Found\nContent-Type:text/plain\n\nFOUR ZERO FOUR\n"

#define BUFLEN     1024
#define PORT       9393 
#define QUEUE_CONN  500
#define MAX_CONN    100 // maximum number of simultaneous incoming HTTP connections

int n_conn;
struct pollfd *conn;
char conn_buffers[MAX_CONN][BUFLEN];
int buf_sizes[MAX_CONN];

static void add_conn (int sd) {
    if (n_conn < MAX_CONN - 1) {
        conn[n_conn].fd = sd;
        conn[n_conn].events = POLLIN;
        n_conn++;
    } else {
        // too many incoming connections!
        // we should modulate the number of poll items to avoid this
    }
}

/* Returns true if the item was removed, false if the operation failed. */
static bool remove_conn (int sd) {
    if (n_conn < 1) {
        return false; // called remove on an empty list
    }
    struct pollfd *pfd, *last = conn + n_conn - 1;
    for (pfd = &conn; pfd <= last; ++pfd) {
        if (pfd->fd == sd) {
            if (pfd != last) {
                // swap last entry into the hole created by removing this socket descriptor
                pfd->fd = last->fd;
                pfd->events = last->events; // not strictly necessary unless events are different
            }
            n_conn--;
            return true;
        }
    }
    return false; // sd was not found
}

/* parameter nc: open HTTP connection number to read from */
static void read_input (int nc) {
    char *buf = conn_buffers[nc];
    char *c = buf + buf_sizes[nc]; // position of the first new character in the buffer
    int remaining = BUFLEN - buf_sizes[nc];
    int received = recv (conn[nc].fd, buf, remaining, 0);
    if (received <= 0) {
        printf ("receive error \n");
        return; //buf_reset (nc);
    }
    buf_sizes[nc] += received;
    if (buf_sizes[nc] >= BUFLEN) {
        printf ("exceeded maximum line length \n");
        return;
    }
    printf ("received: %s \n", c);
    printf ("buffer is now: %s \n", buf);
    for (char *end = c + received; c <= end; ++c) {
        if (*c == '\n' || *c == '\r') {
            *c = '\0';
            
        }  
    }
    char *token = strtok (in, " ");
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
    qstring++;
    char out[BUFLEN];
    strcpy (out, OK_TEXT_PLAIN);
    send(client_socket, out, strlen(out), 0);     
    strcpy (out, qstring);
    send(client_socket, out, strlen(out), 0);     
    close (client_socket);
    continue;
    cleanup: {
        send(client_socket, ERROR_404, strlen(ERROR_404), 0);
        close (client_socket);
    }
    struct sockaddr_in	client_in_addr;
    struct in_addr      client_ip_addr;
}

int main (void) {
    
    // Set up TCP/IP stream socket for incoming HTTP requests
    struct sockaddr_in	server_in_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY)
    };
    // Socket is nonblocking -- connections or bytes may not be waiting
    unsigned int server_socket = socket (AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    socklen_t in_addr_length = sizeof (server_in_addr);
    bind(server_socket, (struct sockaddr *) &server_in_addr, sizeof(server_in_addr));
    listen(server_socket, PENDING);

    // Set up ØMQ socket to communicate with the RRRR broker
    zctx_t *ctx = zctx_new ();
    void *broker_socket = zsocket_new (ctx, ZMQ_DEALER); // full async
    if (zsocket_connect (broker_socket, CLIENT_ENDPOINT)) {
        die ("RRRR OTP REST API server could not connect to broker.");
    }
    
    zmq_pollitem_t poll_items [2 + MAX_CONN];
    // First item is ØMQ socket to/from the RRRR broker
    items[0].socket = broker_socket;
    items[0].events = ZMQ_POLLIN;
    // Second item is standard socket for incoming HTTP requests
    items[1].socket = NULL; 
    items[1].fd = server_socket;
    items[1].events = ZMQ_POLLIN;
    // The rest are incoming HTTP connections
    conn = *(items[2]);
    n_conn = 0;
    
    long n_in = 0, n_out = 0;
    for (;;) {
        // Blocking poll for queued connections and ZMQ broker events and block forever.
        // TODO we should change the number of items if MAX_CONN reached, to stop checking for incoming
        int n_waiting = zmq_poll (items, 2 + n_conn, -1); 
        if (n_waiting < 1) {
            printf ("zmq socket poll error %d\n", n_waiting);
            continue;
        }
        if (items[0].revents & ZMQ_POLLIN) {
            // ØMQ broker socket has a message for us
            // convert to JSON and write out to client socket, then close socket
            n_waiting--;
        }
        for (int c = 0; c < n_conn && n_waiting > 0; ++c) {
            // Read from any incoming HTTP connections that have available input.
            // Check existing connections before adding new ones, since adding will change count.
            // This is potentially inefficient since a single new incoming connection will cause an iteration through the whole list of existing connections.
            if (conn[c].revents & POLLIN) {
                read_input (conn[c].fd);
                --n_waiting;
            }
        }
        if (items[1].revents & ZMQ_POLLIN) {
            // Listening TCP/IP socket has a queued connection
            unsigned int client_socket;
            // Will client sockets necessarily be nonblocking because the listening socket is?
            client_socket = accept(server_socket, (struct sockaddr *) &client_in_addr, &in_addr_length);
            if (client_socket < 0) {
                printf ("error on tcp socket accept \n");
                continue;
            }
            add_conn (client_socket);
            n_waiting--;
        }
    } // end main loop
    close (server_socket);
    return (0);
}

