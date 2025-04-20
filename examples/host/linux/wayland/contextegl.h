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
    class Buffer
    {
    public:
        void resize(uint32_t width, uint32_t height)
        {wl_egl_window_resize(m_egl_window, width, height, 0, 0);}

        bool empty() const {return m_egl_surface == nullptr;}

        void destroy(EGLDisplay dpy)
        {
            if (m_egl_surface){
                eglDestroySurface(dpy, m_egl_surface);
                wl_egl_window_destroy(m_egl_window);
                m_egl_surface = nullptr;
            }
        }

    private:
        wl_egl_window *m_egl_window{nullptr};
        EGLSurface m_egl_surface{nullptr};

        friend class ContextEGL;
    };

    class Config final
    {
    public:
        explicit Config(wl_display *dpy,
                        const Surface &wl,
                        uint32_t width, uint32_t height);

        ~Config()
        {m_buffer.destroy(m_dpy);}

        Buffer move_buffer()
        {
            auto result = m_buffer;
            m_buffer = Buffer();
            return result;
        }

    private:
        EGLDisplay m_dpy;
        EGLConfig m_egl_config;
        Buffer m_buffer;

        friend class ContextEGL;
    };

    void init(const Config &cfg);

    void makeCurrent(const Buffer &buf);
    void flush(Buffer &buf) const;

    ~ContextEGL();

    operator bool() const { return m_egl_display != nullptr; }

    //bool opaque() const;
    EGLDisplay m_egl_display{nullptr};
private:

    EGLContext m_egl_context{nullptr};

};

}

#endif // CONTEXTEGL_H
