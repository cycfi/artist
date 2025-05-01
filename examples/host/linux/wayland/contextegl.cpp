#include "contextegl.h"
#include "core.h"

#include "ganesh/gl/egl/GrGLMakeEGLInterface.h"

#include <GL/gl.h>

#include "../logger.h"

using namespace WL;

ContextEGL::ContextEGL(EGLNativeDisplayType dpy):
    m_display(eglGetDisplay(dpy)),
    m_context(EGL_NO_CONTEXT),
    m_surface(EGL_NO_SURFACE)
{
    EGLint majorVersion;
    EGLint minorVersion;

    if (m_display == EGL_NO_DISPLAY ||
        eglInitialize(m_display, &majorVersion, &minorVersion) != EGL_TRUE) {

        throw std::runtime_error("Can't connect to egl display");
    }

    if (eglBindAPI(EGL_OPENGL_ES_API) != EGL_TRUE)
        throw std::runtime_error("Can't bind  EGL_OPENGL_ES_API");

    Logger::debug("EGL: %i.%i", majorVersion, minorVersion);

    EGLint config_attribs[] = {
        EGL_RED_SIZE, 8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE, 8,
        EGL_DEPTH_SIZE, 1,
        EGL_STENCIL_SIZE, 1,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_ALPHA_SIZE, 8,
        //EGL_NONE, EGL_NONE,
        EGL_NONE
    };

    EGLint num_config;
    if (!eglChooseConfig (m_display, config_attribs, &m_config, 1, &num_config)
        || num_config != 1)
        throw std::runtime_error("Can't choose config");
}

ContextEGL::~ContextEGL()
{
    if (m_display != EGL_NO_DISPLAY){
        if (m_context != EGL_NO_CONTEXT){
            if (m_surface != EGL_NO_SURFACE){
                eglMakeCurrent (m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, m_context);
                eglDestroySurface(m_display, m_surface);
            }   
            eglDestroyContext(m_display, m_context);
        }
        eglTerminate(m_display);
    }
}

// bool ContextEGL::opaque() const
// {
//     EGLint alpha_size;
//     if (!eglGetConfigAttrib(m_egl_display, m_egl_config, EGL_ALPHA_SIZE, &alpha_size))
//         throw std::runtime_error("failed to get alpha size");

//     return !alpha_size;
// }

void ContextEGL::makeCurrent(Buffer &buf)
{
    if (buf.empty()) return;

    if (m_surface != buf.m_egl_surface){

        m_surface = buf.m_egl_surface;

        if (m_context == EGL_NO_CONTEXT) {

            const EGLint context_attribs[] = {
                EGL_CONTEXT_MAJOR_VERSION, 3,
                EGL_CONTEXT_MINOR_VERSION, 0,
                //EGL_CONTEXT_OPENGL_DEBUG, EGL_TRUE,
                EGL_NONE
            };

            m_context = eglCreateContext(m_display, m_config,
                                         EGL_NO_CONTEXT, context_attribs);
            if (!m_context)
                throw std::runtime_error("Can't create egl context");

            if (!eglMakeCurrent(m_display,
                                m_surface,
                                m_surface,
                                m_context))
                throw std::runtime_error("failed to make EGL context current");

            GlSkiaContext::init(GrGLInterfaces::MakeEGL());

            Logger::debug("context GL_VENDOR: %s, GL_RENDERER: %s, GL_VERSION: %s",
                          glGetString(GL_VENDOR),
                          glGetString(GL_RENDERER),
                          glGetString(GL_VERSION));
        }
        else if (!eglMakeCurrent(m_display,
                            buf.m_egl_surface,
                            buf.m_egl_surface,
                            m_context))
                throw std::runtime_error("failed to make EGL context current");
    }
}

void ContextEGL::destroy(Buffer &buf)
{
    if (buf.m_egl_surface != EGL_NO_SURFACE){
        if (buf.m_egl_surface == m_surface){
            eglMakeCurrent(m_display,
                        EGL_NO_SURFACE,
                        EGL_NO_SURFACE,
                        m_context);

            m_surface = EGL_NO_SURFACE;
        }

        buf.m_skia_surface.reset();// ??
        eglDestroySurface(m_display, buf.m_egl_surface);
        
        buf.m_egl_surface = EGL_NO_SURFACE;
    }
}

void ContextEGL::flush(Buffer &buf) const
{
    GlSkiaContext::flush(buf.m_skia_surface);
    eglSwapBuffers(m_display, buf.m_egl_surface);
}

void WL::ContextEGL::Buffer::init(ContextEGL &ctx, const Surface &wl, uint32_t width, uint32_t height)
{
    assert(m_egl_window == nullptr);//duble initialized

    m_egl_window = wl_egl_window_create(wl.c_ptr(), width, height);
    if(!m_egl_window)
        std::runtime_error("Can't create egl window");

    m_egl_surface = eglCreateWindowSurface(ctx.m_display, ctx.m_config,
                                           m_egl_window, nullptr);
    if (m_egl_surface == EGL_NO_SURFACE)
        std::runtime_error("Can't create egl surface");

    ctx.makeCurrent(*this);

    m_skia_surface = ctx.makeSurface(width, height);
    if (!m_skia_surface)
        throw std::runtime_error("Can't create skia context");
}

void WL::ContextEGL::Buffer::resize(ContextEGL &ctx, uint32_t width, uint32_t height)
{
    assert(m_egl_window);// window not initialized

    wl_egl_window_resize(m_egl_window, width, height, 0, 0);
    m_skia_surface = ctx.makeSurface(width, height);
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
