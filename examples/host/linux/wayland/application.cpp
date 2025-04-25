#include "../application.h"
#include "../logger.h"

#include "contextegl.h"
#include "xdgshell.h"


struct Surface::Impl: public WL::Toplevel
{
    Impl(const WL::Display &display, ::Surface* holder, int width, int height):
        WL::Toplevel(display, width, height),
        m_context(display.c_ptr()),
        m_holder(holder),
        m_need_redraw(false),
        m_is_corrupt(false)
    {}

    bool event_process()
    {
        if (m_need_redraw) draw(m_scale);
        return m_is_corrupt;
    }

    void buffer_scale(float scale) override
    {
        Logger::debug("buffer_scale %f", scale);
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

        try {
            if (m_buffer.empty())
            {
                m_width *= m_scale;
                m_height *= m_scale;

                m_buffer.init(*this, m_width, m_height);
                m_context.makeCurrent(m_buffer);

                m_holder->skia_surf = m_context.makeSurface(m_width, m_height);
                m_holder->configure(w, h, SurfaceState::resizing);
                draw(m_scale);
                return;
            }
            else {
                if (s & WL::resizing)
                    state |= SurfaceState::resizing;
                if (s & WL::maximized)
                    state |= SurfaceState::maximized;
                if (s & WL::activated)
                    state |= SurfaceState::activated;
                if (s & WL::fullscreen)
                    state |= SurfaceState::fullscreen;

                if (s & WL::resizing)
                    m_buffer.resize(w, h); 
            }

            m_holder->configure(w, h, state);
            m_need_redraw = true;

        } catch(const std::runtime_error& e){
            Logger::error("Error configure, the window must be closed : %s", e.what());
            m_need_redraw = false;
            m_is_corrupt = true;
        }
    }

    void closed() override
    {
        m_holder->closed();
    }

    WL::ContextEGL::Buffer m_buffer;
    WL::ContextEGL m_context;
    
    ::Surface* m_holder;
    
    bool m_need_redraw;
    bool m_is_corrupt;
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
    DisplayImpl(WL::Display &&dpy):
        syslogger("artist_lib_wayland"),
        m_display(std::move(dpy)){}

    Logger syslogger;
    WL::Display m_display;
   
    Surface::Impl* m_win{nullptr};
    friend class Surface;
};

void Application::DisplayImpl::start_event(bool &isrun)
{
    assert(m_win);

    while (isrun) {
        m_display.event_wait();
        if (m_win->event_process())
            throw std::runtime_error("Window is corrupt");
    }
}

void Application::run()
{
    if (isRuning) return;
    isRuning = true;

    DisplayImpl::inst().start_event(isRuning);
}

Surface::Surface(int width, int height):
    impl{std::make_unique<Impl>(Application::DisplayImpl::inst().display(), this, width, height)}
{
    Application::DisplayImpl::inst().m_win = impl.get();
}

Surface::~Surface()
{
    Application::DisplayImpl::inst().m_win = nullptr;
}

void Surface::setTitle(const char* txt)
{
    impl->setTitle(txt);
}
