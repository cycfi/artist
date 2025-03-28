#include "contextegl.h"
#include "display.h"

#include <GL/gl.h>

using namespace WL;

ContextEGL::ContextEGL(bool transparent):
    m_egl_display(eglGetDisplay(Display::instance().native()))
{
    EGLint majorVersion;
    EGLint minorVersion;

    if (m_egl_display == EGL_NO_DISPLAY ||
        eglInitialize(m_egl_display, &majorVersion, &minorVersion) != EGL_TRUE) {

        throw std::runtime_error("Can't connect to egl display");
    }

    if (eglBindAPI(EGL_OPENGL_ES_API) != EGL_TRUE)
        throw std::runtime_error("Can't bind  EGL_OPENGL_ES_API");

    std::cerr<<"EGL: "<<majorVersion<<"."<<minorVersion<<std::endl;

    EGLint config_attribs[] = {
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_DEPTH_SIZE, 1,
        EGL_STENCIL_SIZE, 1,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_NONE, EGL_NONE, EGL_NONE
    };

    if (transparent) {
        config_attribs[14] = EGL_ALPHA_SIZE;
        config_attribs[15] = 8;
    }

    EGLint num_config;
    if (!eglChooseConfig (m_egl_display, config_attribs, &m_egl_config, 1, &num_config)
        || num_config != 1)
        throw std::runtime_error("Can't choose config");

    const EGLint context_attribs[] = {
        EGL_CONTEXT_MAJOR_VERSION, 3,
        //EGL_CONTEXT_MINOR_VERSION, 0,
        //EGL_CONTEXT_OPENGL_DEBUG, EGL_TRUE,
        EGL_NONE
    };

    m_egl_context = eglCreateContext(m_egl_display, m_egl_config,
                                     EGL_NO_CONTEXT, context_attribs);
    if (!m_egl_context)
        throw std::runtime_error("Can't create egl context");

    if (!eglMakeCurrent(m_egl_display, NULL, NULL, m_egl_context))
       throw std::runtime_error("failed to make EGL context current");

    std::cerr<<"context GL_VENDOR: "<<glGetString(GL_VENDOR)
              <<" GL_RENDERER: "<<glGetString(GL_RENDERER)
              <<" GL_VERSION: " <<glGetString(GL_VERSION)<<std::endl;
}

ContextEGL::~ContextEGL()
{//std::cerr<<"~PaintContext() m_egl_display "<<m_egl_display<<std::endl;
    if (m_egl_display){
        if (m_egl_context){
            eglMakeCurrent (m_egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, m_egl_context);
            eglDestroyContext(m_egl_display, m_egl_context);
        }
        eglTerminate(m_egl_display);
    }
}

bool ContextEGL::opaque() const
{
    EGLint alpha_size;
    if (!eglGetConfigAttrib(m_egl_display, m_egl_config, EGL_ALPHA_SIZE, &alpha_size))
        throw std::runtime_error("failed to get alpha size");

    return !alpha_size;
}

void ContextEGL::makeCurrent(Buffer &buf)
{
    eglMakeCurrent(m_egl_display,
                   buf.m_egl_surface,
                   buf.m_egl_surface,
                   m_egl_context);
}

void ContextEGL::flush(Buffer &buf) const
{
    eglSwapBuffers(m_egl_display, buf.m_egl_surface);
}

void ContextEGL::destroy(Buffer &buf)
{
    if (buf.m_egl_surface)
        eglDestroySurface(m_egl_display, buf.m_egl_surface);
    if (buf.m_egl_window)
        wl_egl_window_destroy(buf.m_egl_window);
    buf.m_egl_surface = nullptr;
}

const char* getEGLErrorString()
{
    switch (eglGetError())
    {
    case EGL_SUCCESS:
        return "Success";
    case EGL_NOT_INITIALIZED:
        return "EGL is not or could not be initialized";
    case EGL_BAD_ACCESS:
        return "EGL cannot access a requested resource";
    case EGL_BAD_ALLOC:
        return "EGL failed to allocate resources for the requested operation";
    case EGL_BAD_ATTRIBUTE:
        return "An unrecognized attribute or attribute value was passed in the attribute list";
    case EGL_BAD_CONTEXT:
        return "An EGLContext argument does not name a valid EGL rendering context";
    case EGL_BAD_CONFIG:
        return "An EGLConfig argument does not name a valid EGL frame buffer configuration";
    case EGL_BAD_CURRENT_SURFACE:
        return "The current surface of the calling thread is a window, pixel buffer or pixmap that is no longer valid";
    case EGL_BAD_DISPLAY:
        return "An EGLDisplay argument does not name a valid EGL display connection";
    case EGL_BAD_SURFACE:
        return "An EGLSurface argument does not name a valid surface configured for GL rendering";
    case EGL_BAD_MATCH:
        return "Arguments are inconsistent";
    case EGL_BAD_PARAMETER:
        return "One or more argument values are invalid";
    case EGL_BAD_NATIVE_PIXMAP:
        return "A NativePixmapType argument does not refer to a valid native pixmap";
    case EGL_BAD_NATIVE_WINDOW:
        return "A NativeWindowType argument does not refer to a valid native window";
    case EGL_CONTEXT_LOST:
        return "The application must destroy all contexts and reinitialise";
    default:
        return "ERROR: UNKNOWN EGL ERROR";
    }
}

ContextEGL::BufferEGL::BufferEGL(ContextEGL &ctx, NativeSurface &wl, uint32_t width, uint32_t height):
    m_egl_window(wl_egl_window_create(wl.c_ptr(), width, height)),
    m_egl_surface(nullptr)
{
    if(m_egl_window){
        m_egl_surface = eglCreateWindowSurface(ctx.m_egl_display,
                                             ctx.m_egl_config,
                                             m_egl_window, nullptr);
        if (!m_egl_surface)
            wl_egl_window_destroy(m_egl_window);
    }
}
