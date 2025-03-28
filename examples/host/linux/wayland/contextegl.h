#ifndef CONTEXTEGL_H
#define CONTEXTEGL_H

//#include <iostream>
#include <wayland-egl-core.h>
#include <EGL/egl.h>

//#include <memory>

namespace WL {

class NativeSurface;
class Display;

class ContextEGL
{
public:
    class BufferEGL
    {
    public:
        BufferEGL(ContextEGL &ctx, NativeSurface &wl, uint32_t width, uint32_t height);

        void resize(uint32_t width, uint32_t height)
        {wl_egl_window_resize(m_egl_window, width, height, 0, 0);}

        bool valid() const {return m_egl_surface != nullptr;}

    protected:
        wl_egl_window *m_egl_window{nullptr};
        EGLSurface m_egl_surface{nullptr};

        friend class ContextEGL;
    };

    using Buffer = BufferEGL;

    void makeCurrent(Buffer &buf);
    void flush(Buffer &buf) const;
    void destroy(Buffer &buf);

    ContextEGL(bool transparent = true);
    ~ContextEGL();

    operator bool() const { return m_egl_display != nullptr; }

    bool opaque() const;

private:
    EGLDisplay m_egl_display;
    EGLContext m_egl_context;
    EGLConfig m_egl_config  ;

    // static inline ContextEGL *s_instance = nullptr;
};

}

#endif // CONTEXTEGL_H
