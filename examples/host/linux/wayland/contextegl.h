#ifndef CONTEXTEGL_H
#define CONTEXTEGL_H

#include "../skia_context.h"

#include <wayland-egl-core.h>
#include <EGL/egl.h>

namespace WL {

class Surface;

class ContextEGL: public GlSkiaContext
{
public:
    explicit ContextEGL(EGLNativeDisplayType dpy);

    class Buffer
    {
    public:
        ~Buffer()
        {if (!empty()) wl_egl_window_destroy(m_egl_window);}

        void init(ContextEGL &ctx, const Surface &wl, uint32_t width, uint32_t height);
        void resize(ContextEGL &ctx, uint32_t width, uint32_t height);

        bool empty() const {return m_egl_window == nullptr;}
        SkCanvas* canvas() const {return m_skia_surface->getCanvas();}

    private:
        wl_egl_window *m_egl_window{nullptr};
        EGLSurface m_egl_surface{EGL_NO_SURFACE};
        sk_sp<SkSurface> m_skia_surface;

        friend class ContextEGL;
    };

    void makeCurrent(Buffer &buf);
    void flush(Buffer &buf) const;
    void destroy(Buffer &buf);

    ~ContextEGL();

    operator bool() const { return m_display != nullptr; }

    //bool opaque() const;
    
private:
    EGLDisplay m_display;
    EGLContext m_context;
    EGLSurface m_surface;
    EGLConfig  m_config;
};

}

#endif // CONTEXTEGL_H
