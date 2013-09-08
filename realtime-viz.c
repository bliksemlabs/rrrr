/* realtime.c */

/*  
    Fetch GTFS-RT updates over Websockets
    Depends on https://github.com/warmcat/libwebsockets
    compile with -lwebsockets -lprotobuf-c
    
    protoc-c --c_out . protobuf/gtfs-realtime.proto    
    clang -O2 -c gtfs-realtime.pb-c.c -o gtfs-realtime.pb-c.o
    clang -O2 realtime.c gtfs-realtime.pb-c.o -o rrrrealtime -lwebsockets -lprotobuf-c

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
#include "gtfs-realtime.pb-c.h"

#include <GL/gl.h>
#include "SDL.h"
#include "wgs84.h"
#include <libshp/shapefil.h>

/* screen width, height, and bit depth */
#define SCREEN_WIDTH  1040
#define SCREEN_HEIGHT 480
#define SCREEN_BPP     16

/* Define our booleans */
#define TRUE  1
#define FALSE 0

SHPHandle hSHP = NULL;

#define ID_LENGTH 32

struct vehicle {
    GLfloat x1, y1;
    GLfloat x2, y2;
    char id[ID_LENGTH];
    int32_t delay;
} vehicles[5000];

unsigned short active = 0;
GLfloat zoom = 1.0f, x_offset = 0.0f, y_offset = 0.0f;
unsigned short current_width, current_height;

int drawGLScene( GLvoid );

void delete_vehicle(char *id) {
	for (int i = 0; i < active; i++) {
		if (strncmp(id, vehicles[i].id, ID_LENGTH) == 0) {
			vehicles[i].id[0] = '\0';
		}
	}
}

void update_vehicle(GLfloat x, GLfloat y, int32_t delay, char *id) {
        int empty = 0;
        for (int i = 0; i < active; i++) {
            if (vehicles[i].id[0] == '\0' && empty == 0)
                empty = i;

            if (strncmp(id, vehicles[i].id, ID_LENGTH) == 0) {
	        vehicles[i].x1 = vehicles[i].x2;
                vehicles[i].y1 = vehicles[i].y2;
                vehicles[i].x2 = x;
                vehicles[i].y2 = y;
                vehicles[i].delay = delay;
                return;
            }
        }
        if (empty == 0 && active >= 0) {
                empty = active;
                active++;
        }

        vehicles[empty].x1 = x;
        vehicles[empty].y1 = y;
        vehicles[empty].x2 = x;
        vehicles[empty].y2 = y;
        vehicles[empty].delay = delay;
	    	printf("%.1f %.1f\n", x, y);
        strncpy(vehicles[empty].id, id, ID_LENGTH);
}

/* This is our SDL surface */
SDL_Surface *surface;

/* function to release/destroy our resources and restoring the old desktop */
void Quit( int returnCode )
{
    /* clean up the window */
    SDL_Quit( );

    /* and exit appropriately */
    exit( returnCode );
}

/* function to reset our viewport after a window resize */
int resizeWindow( int width, int height )
{
    /* Height / width ration */
    GLfloat ratio;

    /* Protect against a divide by zero */
    if ( height == 0 )
        height = 1;

    current_height = height;
    current_height = height;

    ratio = ( GLfloat )width / ( GLfloat )height;

    /* Setup our viewport. */
    glViewport( 0, 0, ( GLsizei )width, ( GLsizei )height );

    /* change to the projection matrix and set our viewing volume. */
    glMatrixMode( GL_PROJECTION );
    glLoadIdentity( );

    /* Set our perspective */
    /* glOrtho( lon2x_d(3.175), lon2x_d(7.295), lat2y_d(50.700), lat2y_d(53.570), -1.0, 1.0 ); */
    GLfloat deltax, deltay;
    if (ratio < 1.3333) {
    	// padding on y, full width
	deltax = 2.850 / zoom;
	deltay = (deltax / ratio) / 1.5;
    } else {
        // padding on x, full height
	deltay = 1.425 / zoom;
	deltax = deltay * ratio * 1.5;
    }
    glOrtho( lon2x_d(x_offset + 5.235 - deltax), lon2x_d(x_offset + 5.235 + deltax), lat2y_d(y_offset + 52.135 - deltay), lat2y_d(y_offset + 52.135 + deltay), -1.0, 1.0 );

    /* Make sure we're chaning the model view and not the projection */
    glMatrixMode( GL_MODELVIEW );

    /* Reset The View */
    glLoadIdentity( );

    return( TRUE );
}

/* function to handle key press events */
void handleKeyPress( SDL_keysym *keysym )
{
    switch ( keysym->sym )
        {
        case SDLK_ESCAPE:
            /* ESC key was pressed */
            Quit( 0 );
            break;
        case SDLK_F1:
            /* F1 key was pressed
             * this toggles fullscreen mode
             */
            SDL_WM_ToggleFullScreen( surface );
            break;
	case SDLK_KP_PLUS:
	    zoom += .1f;
	    resizeWindow(current_width, current_height);
	    drawGLScene();
	    break;
	case SDLK_KP_MINUS:
	    if (zoom > 1) {
  	        zoom -= .1f;
	        resizeWindow(current_width, current_height);
	    	drawGLScene();
            }
	    break;
	case SDLK_LEFT:
	    x_offset -= .1f;
	    resizeWindow(current_width, current_height);
	    drawGLScene();
	    break;
	case SDLK_RIGHT:
	    x_offset += .1f;
	    resizeWindow(current_width, current_height);
	    drawGLScene();
	    break;
	case SDLK_UP:
	    y_offset += .1f;
	    resizeWindow(current_width, current_height);
	    drawGLScene();
	    break;
	case SDLK_DOWN:
	    y_offset -= .1f;
	    resizeWindow(current_width, current_height);
	    drawGLScene();
	    break;

        default:
            break;
        }

    return;
}

/* general OpenGL initialization function */
int initGL( GLvoid )
{
    /* Enable smooth shading */
    glShadeModel( GL_SMOOTH );

    /* Set the background black */
    glClearColor( 0.0f, 0.0f, 0.0f, 0.0f );

    /* Depth buffer setup */
    glClearDepth( 1.0f );

    #if 0
    /* Enables Depth Testing */
    glEnable( GL_DEPTH_TEST ); 

    /* The Type Of Depth Test To Do */
    glDepthFunc( GL_LEQUAL );
    #endif

	glLineWidth(.5);

    /* Really Nice Perspective Calculations */
    glHint( GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST );

    return( TRUE );
}

/* Here goes our drawing code */
int drawGLScene( GLvoid )
{
        if(hSHP && hSHP->nShapeType == SHPT_POLYGON) {
                SHPObject *psShape;
                glColor3f(.4f,.4f,.4f);
                for(int i=0;i<hSHP->nRecords;i++)
                {
                        psShape = SHPReadObject(hSHP, i);
                        if (psShape->nParts > 1) {
                                for (int z=0; z<(psShape->nParts - 1);z++)
                                {
                                        glBegin(GL_LINE_STRIP);
                                        for(int j=psShape->panPartStart[z];j<psShape->panPartStart[z+1];j++)
                                        {
                                                glVertex2f(lon2x_d((GLfloat) psShape->padfX[j]), lat2y_d( (GLfloat) psShape->padfY[j] ));
                                        }
                                        glEnd();
                                }

                                        glEnd();
                        }
                        glBegin(GL_LINE_STRIP);
                        for(int j=psShape->panPartStart[psShape->nParts - 1];j<psShape->nVertices;j++)
                        {
                                glVertex2f(lon2x_d((GLfloat) psShape->padfX[j]), lat2y_d( (GLfloat) psShape->padfY[j] ) );
                        }
                        glEnd();

                        SHPDestroyObject(psShape);
                }
        }

        for (int i = 0; i < active; i++) {
                if (vehicles[i].id[0] != '\0') {
        		glBegin(GL_LINES);
			if (vehicles[i].delay < 0) {
				glColor3f(1.0f,0.0f,0.0f);
			} else if (vehicles[i].delay < 120) {
				glColor3f(0.0f,1.0f,0.0f);
			} else {
				glColor3f(1.0f,0.5f,0.0f);
			}

                        glVertex2f(vehicles[i].x1,vehicles[i].y1);
                        glVertex2f(vehicles[i].x2,vehicles[i].y2);
        		glEnd();
                }
        }

    /* Draw it to the screen */
    SDL_GL_SwapBuffers( );

    return( TRUE );
}

/* 
Websockets exchange frames. Messages can be split across frames.  
Libwebsockets does not aggregate frames into messages, you must do it manually.
"The configuration-time option MAX_USER_RX_BUFFER has been replaced by a
buffer size chosen per-protocol.  For compatibility, there's a default
of 4096 rx buffer, but user code should set the appropriate size for
the protocol frames."
If your frames ever exceed the set size, you need to detect it with libwebsockets_remaining_packet_payload.
See README.coding on how to support fragmented messages (multiple frames per message).
Also libwebsockets-test-fraggle.

Test fragmented messages with /alerts endpoint, since it dumps around 23k in the first message.

http://stackoverflow.com/questions/13010354/chunking-websocket-transmission
http://autobahn.ws/
http://www.lenholgate.com/blog/2011/07/websockets-is-a-stream-not-a-message-based-protocol.html
 */

#define MAX_FRAME_LENGTH (10 * 1024)
#define MAX_MESSAGE_LENGTH (1000 * 1024)

uint8_t msg[MAX_MESSAGE_LENGTH];
size_t msg_len = 0;

static void msg_add_frame (uint8_t *frame, size_t len) { 
    if (msg_len + len > MAX_MESSAGE_LENGTH) {
        fprintf (stderr, "message exceeded maximum message length\n");
        msg_len = 0;
        return;
    }
    memcpy(msg + msg_len, frame, len);
    msg_len += len;
}

static void msg_reset () { msg_len = 0; }

static void msg_dump () {
    for (uint8_t *c = msg; c < msg + msg_len; ++c) {
        if (*c >= 32 && *c < 127) printf ("%c", *c);
        else printf ("[%02X]", *c);
    }
    printf ("\n===============  END OF MESSAGE  ================\n");
}
 
/* Protocol: Incremental GTFS-RT */

static void show_gtfsrt (uint8_t *buf, size_t len) {
    TransitRealtime__FeedMessage *msg;
    msg = transit_realtime__feed_message__unpack (NULL, len, buf);
    if (msg == NULL) {
        fprintf (stderr, "error unpacking incoming gtfs-rt message\n");
        return;
    }
    printf("Received feed message with %lu entities.\n", msg->n_entity);
    for (int e = 0; e < msg->n_entity; ++e) {
        TransitRealtime__FeedEntity *entity = msg->entity[e];
        printf("  entity %d has id %s\n", e, entity->id);
	if (entity->has_is_deleted && entity->is_deleted) {
		delete_vehicle(entity->id);
	} else {
		if (entity->vehicle && entity->vehicle->vehicle && entity->vehicle->position) {
			int32_t delay = 0;
			if (entity->vehicle->ovapi_vehicle_position) {
				delay = entity->vehicle->ovapi_vehicle_position->delay;
			}

			printf("    vehicle id: %s at %.4f, %.4f delay %d\n",
				entity->vehicle->vehicle->id,
				entity->vehicle->position->latitude,
				entity->vehicle->position->longitude,
				delay);
			update_vehicle(lon2x_d((GLfloat) entity->vehicle->position->longitude),
				       lat2y_d((GLfloat) entity->vehicle->position->latitude),
				       delay,
				       entity->id);
		}
	}
    }
    transit_realtime__feed_message__free_unpacked (msg, NULL);
}

static bool socket_closed = false;
static bool force_exit = false;
 
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
        fprintf(stderr, "rx %d bytes: ", (int)len);
        if (libwebsockets_remaining_packet_payload (wsi)) {
            fprintf (stderr, "frame exceeds maximum allowed frame length\n");
        } else if (libwebsocket_is_final_fragment (wsi)) {
            fprintf(stderr, "final frame, ");
            if (msg_len == 0) {
                /* single frame message, nothing in the buffer */
                fprintf(stderr, "single-frame message. ");
                if (len > 0) show_gtfsrt (in, len);
            } else { 
                /* last frame in a multi-frame message */
                fprintf(stderr, "had previous fragment frames. ");
                msg_add_frame (in, len);
                // msg_dump (); // visualize assembled message
                show_gtfsrt (msg, msg_len);
                fprintf(stderr, "emptying message buffer. ");
                msg_reset();
            }
        } else {
            /* non-final fragment frame */
            fprintf(stderr, "message fragment frame. ");
            msg_add_frame (in, len);
        }
        fprintf(stderr, "\n");
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
        0,                 // shared protocol data size
        MAX_FRAME_LENGTH,  // frame buffer size
    },
    { NULL, NULL, 0, 0 } /* end */
};

void sighandler(int sig) {
    force_exit = true;
}

static struct option long_options[] = {
    { "port",    required_argument, NULL, 'p' },
    { "address", required_argument, NULL, 'a' },
    { "path",    required_argument, NULL, 't' },
    { "help",    no_argument,       NULL, 'h' },
    { NULL, 0, 0, 0 } /* end */
};


int main(int argc, char **argv) {
    int ret = 0;
    int port = 8088;
    bool use_ssl = false;
    struct libwebsocket_context *context;
    const char *address = "ovapi.nl";
    const char *path = "/vehiclePositions";
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
            printf ("address set to %s\n", address);
            break;
        case 't':
            path = optarg;
            printf ("path set to %s\n", path);
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


    /* Flags to pass to SDL_SetVideoMode */
    int videoFlags;
    /* main loop variable */
    int done = FALSE;
    /* used to collect events */
    SDL_Event event;
    /* this holds some info about our display */
    const SDL_VideoInfo *videoInfo;
    /* whether or not the window is active */
    int isActive = TRUE;

    /* initialize SDL */
    if ( SDL_Init( SDL_INIT_VIDEO ) < 0 )
        {
            fprintf( stderr, "Video initialization failed: %s\n",
                     SDL_GetError( ) );
            Quit( 1 );
        }

    /* Fetch the video info */
     videoInfo = SDL_GetVideoInfo( );

     if ( !videoInfo )
        {
            fprintf( stderr, "Video query failed: %s\n",
                     SDL_GetError( ) );
            Quit( 1 );
        }

    /* the flags to pass to SDL_SetVideoMode */
    videoFlags  = SDL_OPENGL;          /* Enable OpenGL in SDL */
    videoFlags |= SDL_GL_DOUBLEBUFFER; /* Enable double buffering */
    videoFlags |= SDL_HWPALETTE;       /* Store the palette in hardware */
    videoFlags |= SDL_RESIZABLE;       /* Enable window resizing */

    /* This checks to see if surfaces can be stored in memory */
    if ( videoInfo->hw_available )
        videoFlags |= SDL_HWSURFACE;
    else
        videoFlags |= SDL_SWSURFACE;

    /* This checks if hardware blits can be done */
    if ( videoInfo->blit_hw )
        videoFlags |= SDL_HWACCEL;

    /* Sets up OpenGL double buffering */
    SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );

    /* get a SDL surface */
    surface = SDL_SetVideoMode( SCREEN_WIDTH, SCREEN_HEIGHT, SCREEN_BPP,
                                videoFlags );

    /* Verify there is a surface */
    if ( !surface )
        {
            fprintf( stderr,  "Video mode set failed: %s\n", SDL_GetError( ) );
            Quit( 1 );
        }

    /* initialize OpenGL */
    initGL( );

    /* resize the initial window */
    resizeWindow( SCREEN_WIDTH, SCREEN_HEIGHT );

    hSHP = SHPOpen("shp/NLD_adm1.shp", "rb" );

    /* wait for events */

    /* service the websocket context to handle incoming packets */
    int n = 0;
    while (n >= 0 && !socket_closed && !force_exit) {
	n = libwebsocket_service(context, 500);
           /* handle the events in the queue */

            while ( SDL_PollEvent( &event ) )
                {
                    switch( event.type )
                        {
                        case SDL_ACTIVEEVENT:
                            /* Something's happend with our focus
                             * If we lost focus or we are iconified, we
                             * shouldn't draw the screen
                             */
                            if ( event.active.gain == 0 )
                                isActive = TRUE;
                            else
                                isActive = TRUE;
                            break;
                        case SDL_VIDEORESIZE:
                            /* handle resize event */
                            surface = SDL_SetVideoMode( event.resize.w,
                                                        event.resize.h,
                                                        16, videoFlags );
                            if ( !surface )
                                {
                                    fprintf( stderr, "Could not get a surface after resize: %s\n", SDL_GetError( ) );
                                    Quit( 1 );
                                }
                            resizeWindow( event.resize.w, event.resize.h );
                            break;
                        case SDL_KEYDOWN:
                            /* handle key presses */
                            handleKeyPress( &event.key.keysym );
                            break;
                        case SDL_QUIT:
                            /* handle quit requests */
                            done = TRUE;
                            break;
                        default:
                            break;
                        }
                }

            /* draw the scene */
            if ( isActive )
                drawGLScene( );
        }	

bail:
    fprintf(stderr, "Exiting\n");
    libwebsocket_context_destroy(context);
    SHPClose(hSHP);
    return ret;

usage:
    fprintf(stderr, "Usage: rrrrealtime [--address=<server address>] [--port=<p>] [--path=/<path>]\n");
    return 1;
    
}
