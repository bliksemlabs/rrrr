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
#include <poll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <czmq.h>
#include "util.h"
#include "config.h"

#define OK_TEXT_PLAIN "HTTP/1.0 200 OK\nContent-Type:text/plain\n\n"
#define ERROR_404     "HTTP/1.0 404 Not Found\nContent-Type:text/plain\n\nFOUR ZERO FOUR\n"

#define BUFLEN     1024
#define PORT       9393 
#define QUEUE_CONN  500
#define MAX_CONN    100 // maximum number of simultaneous incoming HTTP connections

uint32_t n_conn;
zmq_pollitem_t *conn;
char conn_buffers[MAX_CONN][BUFLEN];
uint32_t buf_sizes[MAX_CONN];

// Poll items, including zmq and listening/client network sockets
zmq_pollitem_t items [2 + MAX_CONN]; 

uint32_t conn_remove_queue[MAX_CONN];
uint32_t conn_remove_n = 0;

static uint32_t remove_conn_later (uint32_t nc) {
    conn_remove_queue[conn_remove_n] = nc;
    conn_remove_n += 1;
    return conn_remove_n;
}

/* Debug function: print out all the connections. */
static void dump_conns () {
    printf ("number of active connections: %d\n", n_conn);
    for (int i = 0; i < n_conn; ++i) {
        zmq_pollitem_t *c = conn + i;
        char *b = &(conn_buffers[i][0]);
        printf ("connection %02d: fd=%d buf=%s\n", i, c->fd, b);
    }
}

static void add_conn (uint32_t sd) {
    if (n_conn < MAX_CONN - 1) {
        printf ("adding a connection for socket descriptor %d\n", sd);
        conn[n_conn].socket = NULL; // this is a standard socket, not a ZMQ socket
        conn[n_conn].fd = sd;
        conn[n_conn].events = POLLIN;
        n_conn++;
        dump_conns();
    } else {
        // too many incoming connections!
        // we should modulate the number of poll items to avoid this
        printf ("too many incoming connections, dropping one on the floor. \n");
    }
}

/* 
  Returns true if the poll item was removed, false if the operation failed.
  parameter nc: open HTTP connection number to remove/close 
*/
static bool remove_conn (uint32_t nc) {
    if (n_conn < 1) {
        return false; // called remove on an empty list
    }
    if (nc >= n_conn) {
        return false; // trying to remove an inactive connection
    }
    int last_index = n_conn - 1;
    zmq_pollitem_t *item = conn + nc;
    zmq_pollitem_t *last = conn + last_index;
    printf ("removing connection %d with socket descriptor %d\n", nc, item->fd);
    item->fd = last->fd;
    item->events = last->events; // not strictly necessary unless events are different
    // TODO we should also be swapping in the receive buffer
    buf_sizes[last_index] = 0;
    n_conn--;
    dump_conns();
    return true;
}

static void remove_conn_queued () {
    for (int i = 0; i < conn_remove_n; ++i) {
        printf ("removing enqueued connection %d: %d\n", i, conn_remove_queue[i]);
        remove_conn (conn_remove_queue[i]);
    }
    conn_remove_n = 0;
}

/* 
poll tells us that "data is available", which actually means "you can call read on this socket without blocking".
if read/recv then returns 0 bytes, that means the socket is closed.
parameter nc: open HTTP connection number to read from 
*/
static void read_input (uint32_t nc) {
    char *buf = conn_buffers[nc];
    char *c = buf + buf_sizes[nc]; // position of the first new character in the buffer
    uint32_t remaining = BUFLEN - buf_sizes[nc];
    size_t received = recv (conn[nc].fd, buf, remaining, 0);
    if (received == 0) {
        // if recv returns zero, that means the connection has been closed
        printf ("socket %d closed\n", nc);
        remove_conn_later (nc); // don't remove immediately, since we are in the middle of a poll loop.
        return;
    }
    buf_sizes[nc] += received;
    if (buf_sizes[nc] >= BUFLEN) {
        printf ("exceeded maximum line length \n");
        return;
    }
    printf ("received: %s \n", c);
    printf ("buffer is now: %s \n", buf);
    bool eol = false;
    for (char *end = c + received; c <= end; ++c) {
        if (*c == '\n' || *c == '\r') {
            *c = '\0';
            eol = true;
            break;
        }
    }
    if (eol) {
        char *token = strtok (buf, " ");
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
        // at this point, once we have the request, we could remove the poll item while keeping the file descriptor open.
        qstring++;
        char out[BUFLEN];
        strcpy (out, OK_TEXT_PLAIN);
        send (conn[nc].fd, out, strlen(out), 0);     
        strcpy (out, qstring);
        send (conn[nc].fd, out, strlen(out), 0);
        close (conn[nc].fd);
        remove_conn_later (nc);
    }
    return;
    /*
    close (client_socket);
    cleanup: {
        send(client_socket, ERROR_404, strlen(ERROR_404), 0);
        close (client_socket);
    }
    */
    cleanup:
    send(conn[nc].fd, ERROR_404, strlen(ERROR_404), 0);
    remove_conn_later (nc);
}

int main (void) {
    
    // Set up TCP/IP stream socket for incoming HTTP requests
    struct sockaddr_in	server_in_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY)
    };
    
    // Socket is nonblocking -- connections or bytes may not be waiting
    uint32_t server_socket = socket (AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    socklen_t in_addr_length = sizeof (server_in_addr);
    bind(server_socket, (struct sockaddr *) &server_in_addr, sizeof(server_in_addr));
    listen(server_socket, QUEUE_CONN);

    // Set up ØMQ socket to communicate with the RRRR broker
    zctx_t *ctx = zctx_new ();
    void *broker_socket = zsocket_new (ctx, ZMQ_DEALER); // full async
    if (zsocket_connect (broker_socket, CLIENT_ENDPOINT)) {
        die ("RRRR OTP REST API server could not connect to broker.");
    }
    
    zmq_pollitem_t *broker_item = &(items[0]);
    zmq_pollitem_t *http_item   = &(items[1]);
    // First poll item is ØMQ socket to/from the RRRR broker
    broker_item->socket = broker_socket;
    broker_item->fd = 0;
    broker_item->events = ZMQ_POLLIN;
    // Second poll item is a standard socket for incoming HTTP requests
    http_item->socket = NULL; 
    http_item->fd = server_socket;
    http_item->events = ZMQ_POLLIN;
    // The remaining poll items are incoming HTTP connections
    conn = &(items[2]);
    n_conn = 0;
    
    long n_in = 0, n_out = 0;
        for (;;) {
        // Blocking poll for queued connections and ZMQ broker events and block forever.
        // TODO we should change the number of items if MAX_CONN reached, to stop checking for incoming
        int n_waiting = zmq_poll (items, 2 + n_conn, -1); 
        if (n_waiting < 1) {
            printf ("interrupted.\n");
            break;
        }
        if (broker_item->revents & ZMQ_POLLIN) {
            // ØMQ broker socket has a message for us
            // convert to JSON and write out to client socket, then close socket
            --n_waiting;
        }
        for (uint32_t c = 0; c < n_conn && n_waiting > 0; ++c) {
            // Read from any incoming HTTP connections that have available input.
            // Check existing connections before adding new ones, since adding will change count.
            // This is potentially inefficient since a single new incoming connection will cause an iteration through the whole list of existing connections.
            if (conn[c].revents & POLLIN) {
                
                read_input (c);
                --n_waiting;
            }
        }
        // If the fd field is negative, then the corresponding events field is ignored and the revents field returns zero. 
        // This provides an easy way of ignoring a file descriptor for a single poll() call: simply negate the fd field.
        // This way we can ignore incoming connections when we already have too many.
        if (http_item->revents & ZMQ_POLLIN) {
            // Listening TCP/IP socket has a queued connection
            struct sockaddr_in	client_in_addr;
            socklen_t in_addr_length;
            // Will client sockets necessarily be nonblocking because the listening socket is?
            int client_socket = accept(server_socket, (struct sockaddr *) &client_in_addr, &in_addr_length);
            if (client_socket < 0) {
                printf ("error on tcp socket accept \n");
                continue;
            }
            add_conn (client_socket);
            n_waiting--;
        }
        remove_conn_queued (); // remove all connections found to be closed during this iteration.
    } // end main loop
    close (server_socket);
    return (0);
}

