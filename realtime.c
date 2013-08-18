/* realtime.c */

/*  
    Fetch GTFS-RT updates over Websockets
    Depends on https://github.com/warmcat/libwebsockets
    compile with -lwebsockets 
*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <stdbool.h>
#include <getopt.h>
#include <string.h>
#include <signal.h>

#include <libwebsockets.h>

static bool socket_closed = false;
static bool force_exit = false;
 
 
/* Protocol: Incremental GTFS-RT */

static int callback_gtfs_rt (struct libwebsocket_context *this,
			                 struct libwebsocket *wsi,
			                 enum libwebsocket_callback_reasons reason,
                             void *user, void *in, size_t len) {
	switch (reason) {

	case LWS_CALLBACK_CLOSED:
		fprintf(stderr, "LWS_CALLBACK_CLOSED\n");
		socket_closed = 1;
		break;

	case LWS_CALLBACK_CLIENT_RECEIVE:
		((char *)in)[len] = '\0';
		fprintf(stderr, "rx %d '%s'\n", (int)len, (char *)in);
		break;

	case LWS_CALLBACK_CLIENT_CONFIRM_EXTENSION_SUPPORTED:
        return 0; // say everything is supported?
		break;

	default:
		break;
	}
	return 0;
}


/* List of supported protocols and their callbacks */

enum protocols {
	PROTOCOL_INCREMENTAL_GTFS_RT,
	PROTOCOL_COUNT /* end */
};

static struct libwebsocket_protocols protocols[] = {
	{
		"incremental-gtfs-rt",
		callback_gtfs_rt,
		0,
		20,
	},
	{ NULL, NULL, 0, 0 } /* end */
};

void sighandler(int sig) {
	force_exit = true;
}

static struct option long_options[] = {
    { "port",    required_argument, NULL, 'p' },
    { "address", required_argument, NULL, 'a' },
    { "help",    no_argument,       NULL, 'h' },
    { NULL, 0, 0, 0 } /* end */
};


int main(int argc, char **argv) {
	int ret = 0;
	int port = 8088;
	bool use_ssl = false;
	struct libwebsocket_context *context;
	const char *address = "ovapi.nl";
	const char *path = "/tripUpdates";
	struct libwebsocket *wsi_gtfsrt;
	int ietf_version = -1; /* latest */
	struct lws_context_creation_info cc_info;

    int opt = 0;
	while (opt >= 0) {
		opt = getopt_long(argc, argv, "p:a:h", long_options, NULL);
		if (opt < 0) continue;
		switch (opt) {
		case 'p':
			port = atoi(optarg);
			break;
		case 'a':
			address = optarg;
			break;
		case 'h':
			goto usage;
		}
	}

	signal(SIGINT, sighandler);

	/*
	 * create the websockets context.  This tracks open connections and
	 * knows how to route any traffic and which protocol version to use,
	 * and if each connection is client or server side.
	 * This is client-only, so we tell it to not listen on any port.
	 */
    memset (&cc_info, 0, sizeof cc_info);
    cc_info.port = CONTEXT_PORT_NO_LISTEN;
    cc_info.protocols = protocols;
	cc_info.extensions = libwebsocket_get_internal_extensions();
	cc_info.gid = -1;
	cc_info.uid = -1;
	context = libwebsocket_create_context (&cc_info);
	if (context == NULL) {
		fprintf (stderr, "creating libwebsocket context failed\n");
		return 1;
	}

	/* create a client websocket to incremental gtfs-rt distributor */
    wsi_gtfsrt = libwebsocket_client_connect (context, address, port, use_ssl, path, address, address,
                 protocols[PROTOCOL_INCREMENTAL_GTFS_RT].name, ietf_version);
	if (wsi_gtfsrt == NULL) {
		fprintf(stderr, "libwebsocket gtfs-rt connect failed\n");
		ret = 1;
		goto bail;
	}
	fprintf(stderr, "websocket connections opened\n");

	/* service the websocket context to handle incoming packets */
	int n = 0;
	while (n >= 0 && !socket_closed && !force_exit) n = libwebsocket_service(context, 10);

bail:
    fprintf(stderr, "Exiting\n");
    libwebsocket_context_destroy(context);
    return ret;

usage:
    fprintf(stderr, "Usage: rrrrealtime [--address=<server address>] [--port=<p>]\n");
    return 1;
    
}
