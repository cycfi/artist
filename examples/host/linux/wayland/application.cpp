#include "../application.h"

#include "contextegl.h"
#include "xdgshell.h"


struct Surface::Impl: public WL::Toplevel
{
    Impl(const WL::Display &display, ::Surface* holder, int width, int height):
        WL::Toplevel(display, width, height),
        m_display(display),
        m_holder(holder),
        m_need_redraw(false)
    {}

    ~Impl()
    {
        m_buffer.destroy(m_context.m_egl_display);

        std::cout << "DestroyWindow " <<std::endl;
    }

    void event_process()
    {
        if (m_need_redraw) draw(m_scale);
    }

    void buffer_scale(float scale) override
    {
        m_width *= m_scale;
        m_height *= m_scale;

        WL::ContextEGL::Config cfg(m_display.c_ptr(), *this, m_width, m_height);
        m_context.init(cfg);

        m_holder->skia_surf = m_context.makeSurface(m_width, m_height);
        if (m_holder->skia_surf) {
            m_buffer = cfg.move_buffer();
            m_holder->configure(m_width, m_height, SurfaceState::resizing);
            m_need_redraw = true;
        }
    }

    void draw(float scale) override
    {
        m_need_redraw = m_holder->draw(scale);

        m_holder->skia_surf->flush();
        m_context.flush(m_buffer);
    }

    void configure(unsigned w, unsigned h, unsigned s) override
    {
        unsigned state = 0;
        if (s & WL::resizing)
            state |= SurfaceState::resizing;
        if (s & WL::maximized)
            state |= SurfaceState::maximized;
        if (s & WL::activated)
            state |= SurfaceState::activated;
        if (s & WL::fullscreen)
            state |= SurfaceState::fullscreen;

        if (s & WL::resizing){
            m_buffer.resize(w, h); 
        }

        m_holder->configure(w, h, state);
        m_need_redraw = true;
    }

    void closed() override
    {
        m_holder->closed();
    }

    const WL::Display &m_display;
    WL::ContextEGL m_context;
    WL::ContextEGL::Buffer m_buffer;

    ::Surface* m_holder;
    
    bool m_need_redraw;
};

class Application::DisplayImpl
{
public:
    static DisplayImpl& inst()
    {
        static DisplayImpl dpy(WL::Display::create_default<WL::XDGWmBase,
                                                      WL::XDGDecorateManager>());
        return dpy;
    }

    void start_event(bool &isrun);

    const WL::Display& display(){return m_display;}

private:
    DisplayImpl(WL::Display &&dpy): m_display(std::move(dpy)){}

    WL::Display m_display;
    Surface::Impl* m_win{nullptr};
    friend class Surface;
};

void Application::DisplayImpl::start_event(bool &isrun)
{
    assert (m_win);

    while (isrun) {
        m_display.event_wait();
        m_win->event_process(); 
    }
}

void Application::run()
{
    if (isRuning) return;
    isRuning = true;

    DisplayImpl::inst().start_event(isRuning);
}

Surface::~Surface() = default;

Surface::Surface(int width, int height):
    impl{std::make_unique<Impl>(Application::DisplayImpl::inst().display(), this, width, height)}
{
    Application::DisplayImpl::inst().m_win = impl.get();
}

void Surface::setTitle(const char* txt)
{
    impl->setTitle(txt);
}
