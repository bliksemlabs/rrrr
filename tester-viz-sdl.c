/*
 * This code was created by Jeff Molofee '99
 * (ported to Linux/SDL by Ti Leggett '01)
 *
 * If you've found this code useful, please let me know.
 *
 * Visit Jeff at http://nehe.gamedev.net/
 *
 * or for port-specific comments, questions, bugreports etc.
 * email to leggett@eecs.tulane.edu
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <math.h>
#include <string.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glx.h>
#include "SDL.h"
#include "tdata.h"
#include "router.h"
#include "util.h"
#include "wgs84.h"

/* screen width, height, and bit depth */
#define SCREEN_WIDTH  1920
#define SCREEN_HEIGHT 1080
#define SCREEN_BPP     16

/* Set up some booleans */
#define TRUE  1
#define FALSE 0

int drawGLScene( GLvoid );

/* This is our SDL surface */
SDL_Surface *surface;
router_t *router;
tdata_t *tdata;
router_request_t *req;

GLuint  base; /* Base Display List For The Font Set */
GLfloat zoom = 1.0f, x_offset = 5.235f, y_offset = 52.135f;
GLuint zoom_level = 1;
unsigned short current_width, current_height;
GLfloat deltax, deltay;


/* function to recover memory form our list of characters */
GLvoid KillFont( GLvoid )
{
    glDeleteLists( base, 96 );

    return;
}

/* function to release/destroy our resources and restoring the old desktop */
void Quit( int returnCode )
{

    KillFont( );
    /* clean up the window */
    SDL_Quit( );

    /* and exit appropriately */
    // exit( returnCode );
}

/* function to build our font list */
GLvoid buildFont( GLvoid )
{
    Display *dpy;          /* Our current X display */
    XFontStruct *fontInfo; /* Our font info */

    /* Sotrage for 96 characters */
    base = glGenLists( 96 );

    /* Get our current display long enough to get the fonts */
    dpy = XOpenDisplay( NULL );

    /* Get the font information */
    fontInfo = XLoadQueryFont( dpy, "-adobe-helvetica-medium-r-normal--18-*-*-*-p-*-iso8859-1" );

    /* If the above font didn't exist try one that should */
    if ( fontInfo == NULL )
	{
	    fontInfo = XLoadQueryFont( dpy, "fixed" );
	    /* If that font doesn't exist, something is wrong */
	    if ( fontInfo == NULL )
		{
		    fprintf( stderr, "no X font available?\n" );
		    Quit( 1 );
		}
	}

    /* generate the list */
    glXUseXFont( fontInfo->fid, 32, 96, base );

    /* Recover some memory */
    XFreeFont( dpy, fontInfo );

    /* close the display now that we're done with it */
    XCloseDisplay( dpy );

    return;
}

/* Print our GL text to the screen */
GLvoid glPrint( GLfloat cnt1, GLfloat cnt2, const char *fmt, ... )
{
    char text[256]; /* Holds our string */
    va_list ap;     /* Pointer to our list of elements */

    /* If there's no text, do nothing */
    if ( fmt == NULL )
	return;

    /* Position The Text On The Screen */
    glRasterPos2f( cnt1, cnt2 );

    /* Parses The String For Variables */
    va_start( ap, fmt );
      /* Converts Symbols To Actual Numbers */
      vsprintf( text, fmt, ap );
    va_end( ap );

    /* Pushes the Display List Bits */
    glPushAttrib( GL_LIST_BIT );

    /* Sets base character to 32 */
    glListBase( base - 32 );

    /* Draws the text */
    glCallLists( strlen( text ), GL_UNSIGNED_BYTE, text );

    /* Pops the Display List Bits */
    glPopAttrib( );
}

void perspective() {
    /* change to the projection matrix and set our viewing volume. */
    glMatrixMode( GL_PROJECTION );
    glLoadIdentity( );

    /* Height / width ration */
    GLfloat ratio = ( GLfloat ) current_width / ( GLfloat ) current_height;
    /* Set our perspective */
    if (ratio < 1.3333) {
        // padding on y, full width
    deltax = 2.850 / zoom;
    deltay = (deltax / ratio) / 1.5;
    } else {
        // padding on x, full height
    deltay = 1.425 / zoom;
    deltax = deltay * ratio * 1.5;
    }
    glOrtho( lon2x_d(x_offset - deltax), lon2x_d(x_offset + deltax), lat2y_d(y_offset - deltay), lat2y_d(y_offset + deltay), -1.0, 1.0 );
    /* Make sure we're chaning the model view and not the projection */
    glMatrixMode( GL_MODELVIEW );

    /* Reset The View */
    glLoadIdentity( );


}

/* function to reset our viewport after a window resize */
int resizeWindow( int width, int height )
{
    /* Protect against a divide by zero */
    if ( height == 0 )
        height = 1;

    current_height = height;
    current_width = width;

    /* Setup our viewport. */
    glViewport( 0, 0, ( GLsizei )width, ( GLsizei )height );

    perspective();

    return TRUE;
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
        zoom_level ++;
        zoom = pow(2, zoom_level);
        printf("%f\n", zoom);
        perspective();
        break;
    case SDLK_KP_MINUS:
        if (zoom > 1) {
            zoom_level--;
         zoom = pow(2, zoom_level);
            perspective();
            }
        break;
    case SDLK_LEFT:
        x_offset -= (.1f / zoom);
        perspective();
        break;
    case SDLK_RIGHT:
        x_offset += (.1f / zoom);
        perspective();
        break;
    case SDLK_UP:
        y_offset += (.1f / zoom);
        perspective();
        break;
    case SDLK_DOWN:
        y_offset -= (.1f / zoom);
        perspective();
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

    buildFont( );
    return( TRUE );
}

/* Here goes our drawing code */
int drawGLScene( GLvoid )
{
    /* These are to calculate our fps */
    static GLint T0     = 0;
    static GLint Frames = 0;

    /* Clear The Screen And The Depth Buffer */
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
    glLoadIdentity( );

    /* Move into the screen 1 unit */
    // glTranslatef( 0.0f, 0.0f, -1.0f );

    glBegin(GL_POINTS);

    for (uint32_t i = 0; i < tdata->n_stops; i++) {
        if (router->best_time[i] != UNREACHED) {
            GLfloat cnt1 = lon2x_d(tdata->stop_coords[i].lon);
            GLfloat cnt2 = lat2y_d(tdata->stop_coords[i].lat);

            /* Pulsing Colors Based On Text Position */
            // glColor3f( 1.0f *( float )cos( cnt1 ),
            //    1.0f *( float )sin( cnt2 ),
            //    1.0f - 0.5f * ( float )cos( cnt1 + cnt2 ) );

            // http://krazydad.com/tutorials/makecolors.php
            GLfloat frequency = (M_PI * 2) / 6400;
            GLfloat center = 0.5f;
            GLfloat width = 0.5f;
            GLint phase = 0;

            GLint delta = router->best_time[i] - req->time;

            glColor3f( 1.0f * ( float )sin(frequency * delta + 2 + phase) * width + center,
                       1.0f * ( float )sin(frequency * delta + 0 + phase) * width + center,
                       1.0f * ( float )sin(frequency * delta + 4 + phase) * width + center );

            glVertex2f(cnt1, cnt2);

            /* Print text to the screen */
                glPrint( cnt1, cnt2, "%.2d:%.2d", router->best_time[i] / 900, (router->best_time[i] - ((router->best_time[i] / 900) * 900) ) / 15 ); // still in r_time
            // }
        }
    }

    glEnd();
    /* Print text to the screen */

    glColor3f(0.4f, 0.4f, 0.4f);

    if (zoom_level >= 6) {
        for (uint32_t i = 0; i < tdata->n_stops; i++) {
            if (router->best_time[i] != UNREACHED) {
                GLfloat cnt1 = lon2x_d(tdata->stop_coords[i].lon);
                GLfloat cnt2 = lat2y_d(tdata->stop_coords[i].lat);
                glPrint( cnt1, cnt2, "%.2d:%.2d", router->best_time[i] / 900, (router->best_time[i] - ((router->best_time[i] / 900) * 900) ) / 15 ); // still in r_time
            }
        }
    }

    glColor3f(1.0f, 1.0f, 1.0f);
    glBegin(GL_LINE_LOOP);
    for(int i =0; i <= 300; i++){
    double angle = 2 * M_PI * i / 300;
    double x = (0.12f * cos(angle)) + lon2x_d(tdata->stop_coords[req->from].lon);
    double y = (0.12f * sin(angle)) + lat2y_d(tdata->stop_coords[req->from].lat);
    glVertex2d(x,y);
    }
    glEnd();

    // glPrint(lon2x_d(3.0f), lat2y_d(52.135f), "%.2d:%.2d", req->time / 900, (req->time - ((req->time / 900) * 900) ) / 15 ); // still in r_time
    glPrint(lon2x_d(tdata->stop_coords[req->from].lon), lat2y_d(tdata->stop_coords[req->from].lat), "%.2d:%.2d", req->time / 900, (req->time - ((req->time / 900) * 900) ) / 15 ); // still in r_time

    /* Draw it to the screen */
    SDL_GL_SwapBuffers( );

    /* Gather our frames per second */
    Frames++;
    {
	GLint t = SDL_GetTicks();
	if (t - T0 >= 5000) {
	    GLfloat seconds = (t - T0) / 1000.0;
	    GLfloat fps = Frames / seconds;
	    printf("%d frames in %g seconds = %g FPS\n", Frames, seconds, fps);
	    T0 = t;
	    Frames = 0;
	}
    }

    return( TRUE );
}

int sdl_main( router_t *prouter, tdata_t *ptdata, router_request_t *preq )
{
    router = prouter;
    tdata = ptdata;
    req = preq;

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

    /* wait for events */
    while ( !done )
	{
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
				isActive = FALSE;
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
                isActive = TRUE;
			    break;
            case SDL_MOUSEBUTTONDOWN:
                {
                    int x = event.motion.x;
                    int y = event.motion.y;
                    GLdouble ox=0.0,oy=0.0,oz=0.0;
                    GLint viewport[4];
                    GLdouble modelview[16],projection[16];
                    GLfloat wx=x,wy,wz;
                    glGetIntegerv(GL_VIEWPORT,viewport);
                    y=viewport[3]-y;
                    wy=y;
                    glGetDoublev(GL_MODELVIEW_MATRIX,modelview);
                    glGetDoublev(GL_PROJECTION_MATRIX,projection);
                    glReadPixels(x, y,1,1,GL_DEPTH_COMPONENT,GL_FLOAT,&wz);
                    gluUnProject(wx,wy,wz,modelview,projection,viewport,&ox,&oy,&oz);


                    if (event.button.button == SDL_BUTTON_LEFT) {
                        GLfloat dist = 99999.0f;
                        uint32_t nearest = 0;

                        for (uint32_t i = 0; i < tdata->n_stops; i++) {
                            GLfloat cnt1 = lon2x_d(tdata->stop_coords[i].lon);
                            GLfloat cnt2 = lat2y_d(tdata->stop_coords[i].lat);

                            GLfloat cur_dist = ((ox - cnt1) * (ox - cnt1)) + ((oy - cnt2) * (oy - cnt2));

                            if (cur_dist < dist) {
                                dist = cur_dist;
                                nearest = i;
                            }
                        }

                        req->from = nearest;
                    } else if (event.button.button == 4) {

                        printf("%f %f %f %f\n", oy, y_offset, y2lat_d(oy), deltay);
                        x_offset = x2lon_d(ox);
                        y_offset = y2lat_d(oy);

                        zoom_level++;
                        zoom = pow(2, zoom_level);
                        perspective();
                    } else if (event.button.button == 5) {
                        if (zoom > 1) {
                            x_offset = x2lon_d(ox);
                            y_offset = y2lat_d(oy);

                            zoom_level--;
                            zoom = pow(2, zoom_level);
                            perspective();
                        }
                    }
                    break;
            }
			case SDL_QUIT:
			    /* handle quit requests */
			    done = TRUE;
			    break;
			default:
			    break;
			}
		}

	    /* draw the scene */
	    if ( isActive ) {
            router_request_next (req);

            router_route (router, req);
            //uint32_t n_reversals = req->arrive_by ? 1 : 2;
            //if (req->start_trip_trip != NONE) n_reversals = 0;
            //for (uint32_t i = 0; i < n_reversals; ++i) {
            //    router_request_reverse (router, req);
            //    router_route (router, req);
            //}

    		drawGLScene( );
        }
	}

    /* clean ourselves up and exit */
    Quit( 0 );

    /* Should never get here */
    return( 0 );
}
