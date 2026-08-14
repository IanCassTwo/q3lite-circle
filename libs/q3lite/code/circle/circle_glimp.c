/*
===========================================================================
Copyright (C) 1999-2010 id Software LLC, a ZeniMax Media company.
Copyright (C) 2026 Ian Cass

This file is part of Q3lite Source Code.

Q3lite Source Code is free software; you can redistribute it
and/or modify it under the terms of the GNU General Public License as
published by the Free Software Foundation; either version 3 of the License,
or (at your option) any later version.

Q3lite Source Code is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Q3lite Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, Q3lite Source Code is also subject to certain additional terms.
You should have received a copy of these additional terms immediately following
the terms and conditions of the GNU General Public License.  If not, please
request a copy in writing from id Software at the address below.
If you have questions concerning this license or the applicable additional
terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc.,
Suite 120, Rockville, Maryland 20850 USA.
===========================================================================
*/

// Standard C / System headers FIRST (loads Newlib types first)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Keep qgl names as symbols in this implementation unit.
#define QGL_EXTERN

// Quake 3 core headers SECOND
#include "../renderercommon/tr_common.h"
#include "../sys/sys_local.h"

// Define _PTHREAD_TYPES_H_ to prevent Newlib from re-defining pthread types if loaded again
#ifndef _PTHREAD_TYPES_H_
#define _PTHREAD_TYPES_H_
#endif

// Broadcom VideoCore / Khronos headers LAST
#include <bcm_host.h>
#include <EGL/egl.h>
#include <GLES/gl.h>

// Dedicated Circle EGL subsystem state
typedef struct
{
    uint32_t screen_width;
    uint32_t screen_height;
    DISPMANX_DISPLAY_HANDLE_T dispman_display;
    DISPMANX_ELEMENT_HANDLE_T dispman_element;
    EGLDisplay display;
    EGLSurface surface;
    EGLContext context;
} CIRCLE_GL_STATE_T;

static CIRCLE_GL_STATE_T gl_state;

int qglMajorVersion = 1, qglMinorVersion = 1;
int qglesMajorVersion = 1, qglesMinorVersion = 1;

// Define qgl pointers here; they are assigned during GLES init.
#define GLE(ret, name, ...) name##proc * qgl##name;
QGL_1_1_PROCS;
QGL_1_1_FIXED_FUNCTION_PROCS;
QGL_ES_1_1_PROCS;
QGL_ES_1_1_FIXED_FUNCTION_PROCS;
#undef GLE

void (APIENTRYP qglActiveTextureARB) (GLenum texture) = NULL;
void (APIENTRYP qglClientActiveTextureARB) (GLenum texture) = NULL;
void (APIENTRYP qglMultiTexCoord2fARB) (GLenum target, GLfloat s, GLfloat t) = NULL;
void (APIENTRYP qglLockArraysEXT) (GLint first, GLsizei count) = NULL;
void (APIENTRYP qglUnlockArraysEXT) (void) = NULL;

static unsigned long frame_counter = 0;

void myglMultiTexCoord2f( GLenum texture, GLfloat s, GLfloat t )
{
    glMultiTexCoord4f(texture, s, t, 0, 1);
}

/*
===============
GLimp_Shutdown
===============
*/
void GLimp_Shutdown( void )
{
    DISPMANX_UPDATE_HANDLE_T dispman_update;

    ri.Printf( PRINT_ALL, "[VIDEO] GLimp_Shutdown()...\n" );

    ri.IN_Shutdown();

    if (gl_state.display != EGL_NO_DISPLAY)
    {
        qglClear( GL_COLOR_BUFFER_BIT );
        eglSwapBuffers( gl_state.display, gl_state.surface );

        eglMakeCurrent( gl_state.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT );

        if (gl_state.surface != EGL_NO_SURFACE)
            eglDestroySurface( gl_state.display, gl_state.surface );

        if (gl_state.context != EGL_NO_CONTEXT)
            eglDestroyContext( gl_state.display, gl_state.context );

        eglTerminate( gl_state.display );
    }

    if (gl_state.dispman_element != 0)
    {
        dispman_update = vc_dispmanx_update_start( 0 );
        vc_dispmanx_element_remove( dispman_update, gl_state.dispman_element );
        vc_dispmanx_update_submit_sync( dispman_update );
    }

    if (gl_state.dispman_display != 0)
    {
        vc_dispmanx_display_close( gl_state.dispman_display );
    }

    Com_Memset( &gl_state, 0, sizeof( gl_state ) );
    ri.Printf( PRINT_ALL, "[VIDEO] GLimp_Shutdown() complete.\n" );
}

/*
===============
GLimp_Minimize
===============
*/
void GLimp_Minimize( void )
{
    // NOP for bare-metal OS
}

/*
===============
GLimp_LogComment
===============
*/
void GLimp_LogComment( char *comment )
{
}

/*
===============
GLimp_InitExtensions
===============
*/
static void GLimp_InitExtensions( void )
{
    if ( !r_allowExtensions->integer )
    {
        ri.Printf( PRINT_ALL, "* IGNORING OPENGL EXTENSIONS *\n" );
        return;
    }

    ri.Printf( PRINT_ALL, "Initializing OpenGL extensions\n" );

    glConfig.textureCompression = TC_NONE;
    glConfig.textureEnvAddAvailable = qtrue;

    // Multitexture mapping
    qglGetIntegerv( GL_MAX_TEXTURE_UNITS, &glConfig.numTextureUnits );
    if ( glConfig.numTextureUnits > 1 )
    {
        qglMultiTexCoord2fARB = myglMultiTexCoord2f;
        qglActiveTextureARB = (void (APIENTRYP)(GLenum))glActiveTexture;
        qglClientActiveTextureARB = (void (APIENTRYP)(GLenum))glClientActiveTexture;
        ri.Printf( PRINT_ALL, "...using GL_ARB_multitexture (%i texture units)\n", glConfig.numTextureUnits );
    }
    else
    {
        qglMultiTexCoord2fARB = NULL;
        qglActiveTextureARB = NULL;
        qglClientActiveTextureARB = NULL;
        ri.Printf( PRINT_ALL, "...not using GL_ARB_multitexture, < 2 texture units\n" );
    }

    textureFilterAnisotropic = qfalse;
}

/*
===============
GLimp_Init
===============
*/
void GLimp_Init( qboolean fixedFunction )
{
    int32_t success = 0;
    EGLBoolean result;
    EGLint num_config;
    static EGL_DISPMANX_WINDOW_T nativewindow;
    DISPMANX_UPDATE_HANDLE_T dispman_update;
    VC_RECT_T dst_rect;
    VC_RECT_T src_rect;
    VC_DISPMANX_ALPHA_T dispman_alpha;

    static const EGLint attribute_list[] =
    {
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 24,
        EGL_STENCIL_SIZE, 8,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES_BIT,
        EGL_NONE
    };

/*
    static const EGLint attribute_list[] =
{
    EGL_RED_SIZE, 5,
    EGL_GREEN_SIZE, 6,
    EGL_BLUE_SIZE, 5,
    EGL_ALPHA_SIZE, 0,    
    EGL_DEPTH_SIZE, 16,   
    EGL_STENCIL_SIZE, 0,  
    EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES_BIT,
    EGL_NONE
};
*/

    static const EGLint context_attributes[] =
    {
        EGL_CONTEXT_CLIENT_VERSION, 1,
        EGL_NONE
    };

    EGLConfig config;

    ri.Printf( PRINT_ALL, "[VIDEO] Initializing Circle Bare-Metal EGL Display...\n" );

    bcm_host_init();

    // Initialize EGL Display Connection
    gl_state.display = eglGetDisplay( EGL_DEFAULT_DISPLAY );
    if ( gl_state.display == EGL_NO_DISPLAY )
    {
        ri.Error( ERR_FATAL, "GLimp_Init() - eglGetDisplay failed (0x%lx)", (long)eglGetError() );
    }

    result = eglInitialize( gl_state.display, NULL, NULL );
    if ( result == EGL_FALSE )
    {
        ri.Error( ERR_FATAL, "GLimp_Init() - eglInitialize failed (0x%lx)", (long)eglGetError() );
    }

    // Choose Config
    result = eglChooseConfig( gl_state.display, attribute_list, &config, 1, &num_config );
    if ( result == EGL_FALSE || num_config < 1 )
    {
        ri.Error( ERR_FATAL, "GLimp_Init() - eglChooseConfig failed (num_config=%ld, err=0x%lx)", (long)num_config, (long)eglGetError() );
    }

    // Create Rendering Context (GLES 1.x)
    eglBindAPI( EGL_OPENGL_ES_API );
    gl_state.context = eglCreateContext( gl_state.display, config, EGL_NO_CONTEXT, context_attributes );
    if ( gl_state.context == EGL_NO_CONTEXT )
    {
        ri.Error( ERR_FATAL, "GLimp_Init() - eglCreateContext failed (0x%lx)", (long)eglGetError() );
    }

    // -------------------------------------------------------------------
    // RESOLUTION HANDLING: Match Quake 3's r_mode
    // -------------------------------------------------------------------
    uint32_t native_display_width, native_display_height;
    success = graphics_get_display_size( 0 /* LCD/HDMI */, &native_display_width, &native_display_height );
    if ( success < 0 )
    {
        ri.Error( ERR_FATAL, "GLimp_Init() - graphics_get_display_size failed (%ld)", (long)success );
    }

    // Map r_mode integer to resolution dimensions
    int mode = ri.Cvar_VariableIntegerValue( "r_mode" );
    switch ( mode )
    {
        case 0:  gl_state.screen_width = 320;  gl_state.screen_height = 240;  break;
        case 1:  gl_state.screen_width = 400;  gl_state.screen_height = 300;  break;
        case 2:  gl_state.screen_width = 512;  gl_state.screen_height = 384;  break;
        case 3:  gl_state.screen_width = 640;  gl_state.screen_height = 480;  break;
        case 4:  gl_state.screen_width = 800;  gl_state.screen_height = 600;  break;
        case 5:  gl_state.screen_width = 960;  gl_state.screen_height = 720;  break;
        case 6:  gl_state.screen_width = 1024; gl_state.screen_height = 768;  break;
        case 7:  gl_state.screen_width = 1152; gl_state.screen_height = 864;  break;
        case 8:  gl_state.screen_width = 1280; gl_state.screen_height = 1024; break;
        case -1: // Custom Mode
            gl_state.screen_width  = ri.Cvar_VariableIntegerValue( "r_customwidth" );
            gl_state.screen_height = ri.Cvar_VariableIntegerValue( "r_customheight" );
            if ( gl_state.screen_width <= 0 )  gl_state.screen_width = 800;
            if ( gl_state.screen_height <= 0 ) gl_state.screen_height = 600;
            break;
        default: // Default fallback (r_mode 4 or -2/native fallback)
            gl_state.screen_width = 800;
            gl_state.screen_height = 600;
            break;
    }

    ri.Printf( PRINT_ALL, "[VIDEO] Display: %ldx%ld Native -> %ldx%ld Q3 Render Buffer\n", 
               (long)native_display_width, (long)native_display_height, 
               (long)gl_state.screen_width, (long)gl_state.screen_height );

    // Destination rect fills the native HDMI display monitor
    vc_dispmanx_rect_set( &dst_rect, 0, 0, native_display_width, native_display_height );
    
    // Source rect represents the GLES framebuffer (fixed-point 16.16 shift format required by VideoCore)
    vc_dispmanx_rect_set( &src_rect, 0, 0, gl_state.screen_width << 16, gl_state.screen_height << 16 );

    gl_state.dispman_display = vc_dispmanx_display_open( 0 );
    dispman_update = vc_dispmanx_update_start( 0 );

    dispman_alpha.flags = DISPMANX_FLAGS_ALPHA_FIXED_ALL_PIXELS;
    dispman_alpha.opacity = 255;
    dispman_alpha.mask = 0;

    gl_state.dispman_element = vc_dispmanx_element_add(
        dispman_update, 
        gl_state.dispman_display,
        0/*layer*/, 
        &dst_rect, 
        0/*src handle*/,
        &src_rect, 
        DISPMANX_PROTECTION_NONE, 
        &dispman_alpha,
        0/*clamp*/, 
        0/*transform*/
    );

    nativewindow.element = gl_state.dispman_element;
    nativewindow.width = gl_state.screen_width;
    nativewindow.height = gl_state.screen_height;
    vc_dispmanx_update_submit_sync( dispman_update );

    // Create Surface and Make Current
    gl_state.surface = eglCreateWindowSurface( gl_state.display, config, &nativewindow, NULL );
    if ( gl_state.surface == EGL_NO_SURFACE )
    {
        ri.Error( ERR_FATAL, "GLimp_Init() - eglCreateWindowSurface failed (0x%lx)", (long)eglGetError() );
    }

    result = eglMakeCurrent( gl_state.display, gl_state.surface, gl_state.surface, gl_state.context );
    if ( result == EGL_FALSE )
    {
        ri.Error( ERR_FATAL, "GLimp_Init() - eglMakeCurrent failed (0x%lx)", (long)eglGetError() );
    }

    // Disable VSync synchronization delays if desired
    eglSwapInterval( gl_state.display, 0 );

    // Bind QGL Pointers using Q3lite's macro tables
#define GLE(ret, name, ...) qgl##name = gl##name;
    QGL_1_1_PROCS;
    QGL_1_1_FIXED_FUNCTION_PROCS;
    QGL_ES_1_1_PROCS;
    QGL_ES_1_1_FIXED_FUNCTION_PROCS;
#undef GLE

    qglViewport(0, 0, (GLsizei)gl_state.screen_width, (GLsizei)gl_state.screen_height);
    qglScissor(0, 0, (GLsizei)gl_state.screen_width, (GLsizei)gl_state.screen_height);

    // Populate engine glConfig struct
    glConfig.vidWidth = gl_state.screen_width;
    glConfig.vidHeight = gl_state.screen_height;
    glConfig.windowAspect = (float)glConfig.vidWidth / (float)glConfig.vidHeight;
    glConfig.colorBits = 32;
    glConfig.depthBits = 24;
    glConfig.stencilBits = 8;
    glConfig.isFullscreen = qtrue;
    glConfig.driverType = GLDRV_ICD;
    glConfig.hardwareType = GLHW_GENERIC;
    glConfig.deviceSupportsGamma = qfalse;

    // Retrieve Driver Strings
    Q_strncpyz( glConfig.vendor_string, (char *) qglGetString (GL_VENDOR), sizeof( glConfig.vendor_string ) );
    Q_strncpyz( glConfig.renderer_string, (char *) qglGetString (GL_RENDERER), sizeof( glConfig.renderer_string ) );
    Q_strncpyz( glConfig.version_string, (char *) qglGetString (GL_VERSION), sizeof( glConfig.version_string ) );
    Q_strncpyz( glConfig.extensions_string, (char *) qglGetString (GL_EXTENSIONS), sizeof( glConfig.extensions_string ) );

    ri.Printf( PRINT_ALL, "[VIDEO] GL Vendor:   %s\n", glConfig.vendor_string );
    ri.Printf( PRINT_ALL, "[VIDEO] GL Renderer: %s\n", glConfig.renderer_string );
    ri.Printf( PRINT_ALL, "[VIDEO] GL Version:  %s\n", glConfig.version_string );

    GLimp_InitExtensions();

    ri.IN_Init( NULL );

    // FORCE CVARS TO PREVENT BLACK SCREEN / BROKEN SOFTWARE GAMMA
    ri.Cvar_Set( "r_ignorehwgamma", "1" );
    ri.Cvar_Set( "r_gamma", "1.0" );
    ri.Cvar_Set( "r_intensity", "1.0" );
    ri.Cvar_Set( "r_mapOverBrightBits", "0" );
    ri.Cvar_Set( "r_overBrightBits", "0" );
}

/*
===============
GLimp_EndFrame
===============
*/
void GLimp_EndFrame( void )
{
    frame_counter++;

    if ( gl_state.display != EGL_NO_DISPLAY && gl_state.surface != EGL_NO_SURFACE )
    {
        // GL_DEPTH_EXT / GL_STENCIL_EXT are the required enums for the default window framebuffer
        const GLenum attachments[] = { GL_DEPTH_EXT, GL_STENCIL_EXT };
        glDiscardFramebufferEXT( GL_FRAMEBUFFER_OES, 2, attachments );
        //const GLenum attachments[] = { GL_DEPTH_EXT };
        //glDiscardFramebufferEXT( GL_FRAMEBUFFER_OES, 1, attachments );

        // Check for GL errors AFTER discard runs
        // TODO ifdef this out for release builds, since it is expensive to check every frame
        /*
        GLenum err = qglGetError();
        if (err != GL_NO_ERROR)
        {
            ri.Printf(PRINT_ALL, "[VIDEO] GL ERROR before swap = 0x%X\n", err);
        }
        */

        EGLBoolean res = eglSwapBuffers( gl_state.display, gl_state.surface );
        if ( res == EGL_FALSE )
        {
            ri.Printf( PRINT_ALL, "[VIDEO ERROR] eglSwapBuffers failed at frame %lu! Error: 0x%lx\n", 
                        frame_counter, (long)eglGetError() );
        }
    }
    else
    {
        ri.Printf( PRINT_ALL, "[VIDEO ERROR] GLimp_EndFrame called with invalid display/surface!\n" );
    }
}