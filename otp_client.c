// test client

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

#define PORT   9393 
#define BUFLEN 1024

int main (int argc, char **argv) {    
    uint32_t iterations = 10;
    if (argc > 1)
        iterations = atoi (argv[1]);
    struct sockaddr_in	server_in_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(PORT),
        .sin_addr.s_addr = htonl(INADDR_LOOPBACK)
    };
    for (uint32_t i = 0; i < iterations; ++i) {
        uint32_t server_socket = socket (AF_INET, SOCK_STREAM, 0);
        if (connect (server_socket, (struct sockaddr *) &server_in_addr, sizeof(server_in_addr))) {
            printf ("failed to connect socket \n");
            exit (1);
        }
        char in[BUFLEN];
        char out[BUFLEN];
        strcpy (out, "GET http://localhost:9393/plan?12345 HTTP/1.1\n");
        send (server_socket, out, strlen(out), 0);     
        printf ("sent: %s \n", out);
        recv (server_socket, in, BUFLEN, 0);
        printf ("received: %s \n", in);
        // cleanup:
        close (server_socket);
    }   
    return (0);
}


