#include "../application.h"
#include "../skia_context.h"
#include "../logger.h"

#include "ganesh/gl/glx/GrGLMakeGLXInterface.h"

#include <X11/Xlib.h>
#include <cassert>
#include <cstring>
#include <sys/timerfd.h>
#include <unistd.h>

#include <GL/gl.h>
#include <GL/glx.h>

#include <poll.h>


namespace X11 {

    class ContextGLX final: public GlSkiaContext
    {
    public:
        struct Buffer final
        {
            Window win_id{0};
            Colormap colormap{0};

            void destroy(Display* dpy)
            {
                if (win_id){
                    XFreeColormap (dpy, colormap);
                    XDestroyWindow(dpy, win_id);
                }
            }
        };

        struct Config final
        {
            explicit Config(Display* dpy, int width, int height);
            ~Config()
            {m_buffer.destroy(x11_dpy);}

            Buffer move_buffer() 
            {
                auto id = m_buffer.win_id;
                m_buffer.win_id = 0;

                return {id, m_buffer.colormap};
            }
        
        private:
            Display* x11_dpy;
            GLXFBConfig fb_config;
            Buffer m_buffer;

            friend class ContextGLX;
        };

        void init(const Config &cfg);
        void makeCurrent(const Buffer &buf);
        void flush(Buffer &buf);

        ~ContextGLX();

        Display* x11_display{nullptr};

    private:
        GLXContext m_context{nullptr};
    };

    ContextGLX::Config::Config(Display* dpy, int width, int height):
        x11_dpy(dpy)
    {
        auto screen_id = DefaultScreen(dpy);

        GLint majorGLX, minorGLX = 0;
        glXQueryVersion(dpy, &majorGLX, &minorGLX);

        Logger::debug("GLX: %i.%i", majorGLX, minorGLX);

        GLint glxAttribs[] = {
            GLX_X_RENDERABLE    , True,
            GLX_DRAWABLE_TYPE   , GLX_WINDOW_BIT,
            GLX_RENDER_TYPE     , GLX_RGBA_BIT,
            GLX_X_VISUAL_TYPE   , GLX_TRUE_COLOR,
            GLX_RED_SIZE        , 8,
            GLX_GREEN_SIZE      , 8,
            GLX_BLUE_SIZE       , 8,
            GLX_ALPHA_SIZE      , 8,
            GLX_DEPTH_SIZE      , 24,
            GLX_STENCIL_SIZE    , 8,
            GLX_DOUBLEBUFFER    , True,
            None
        };

        XVisualInfo *info{nullptr};
        int fbcount;
        GLXFBConfig* fbc = glXChooseFBConfig(dpy, screen_id, glxAttribs, &fbcount);
        if (fbc == 0)
            throw std::runtime_error("Failed to retrieve framebuffer.");

        // Pick the FB config/visual with the most samples per pixel
        int best_fbc = -1, best_num_samp = -1;
        for (int i = 0; i < fbcount; ++i) {
            XVisualInfo *vi = glXGetVisualFromFBConfig(dpy, fbc[i]);
            if (vi) {
                int samp_buf, samples;
                glXGetFBConfigAttrib( dpy, fbc[i], GLX_SAMPLE_BUFFERS, &samp_buf );
                glXGetFBConfigAttrib( dpy, fbc[i], GLX_SAMPLES       , &samples  );

                if ( best_fbc < 0 || (samp_buf && samples > best_num_samp) ) {
                    best_fbc = i;
                    best_num_samp = samples;
                    if (info)
                        XFree(info);
                    info = vi;
                }
                else
                    XFree(vi);
            }
        }

        if (best_fbc < 0)
            throw std::runtime_error("Failed to get a valid GLX visual");

        fb_config = fbc[best_fbc];
        XFree(fbc);

        Window window_root_id = RootWindow(dpy, screen_id);
        m_buffer.colormap = XCreateColormap(dpy, window_root_id, info->visual, AllocNone);

        XSetWindowAttributes windowAttribs;
        windowAttribs.event_mask = ExposureMask | KeyPressMask | StructureNotifyMask;
        //windowAttribs.border_pixel = WhitePixel(m_display, screen_id);
        //windowAttribs.background_pixel = WhitePixel(dpyx.display, screen_id);
        //windowAttribs.override_redirect = True;
        windowAttribs.colormap = m_buffer.colormap;

        m_buffer.win_id = XCreateWindow(dpy,
                                window_root_id,
                                0,
                                0,
                                width,
                                height,
                                0,
                                info->depth,
                                InputOutput,
                                info->visual,
                                CWColormap | CWEventMask, // | CWBackPixel | CWBorderPixel,
                                &windowAttribs);
        XFree(info);
    }

    void ContextGLX::init(const Config &cfg)
    {
        assert(!m_context); // double initialization

        x11_display = cfg.x11_dpy;

        // Create GLX OpenGL context
        typedef GLXContext (*glXCreateContextAttribsARBProc)(Display*, GLXFBConfig, GLXContext, Bool, const int*);
        glXCreateContextAttribsARBProc glXCreateContextAttribsARB = 0;
        glXCreateContextAttribsARB =
            (glXCreateContextAttribsARBProc) glXGetProcAddressARB( (const GLubyte*) "glXCreateContextAttribsARB" );

        int context_attribs[] = {
            GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
            GLX_CONTEXT_MINOR_VERSION_ARB, 0,
            //GLX_CONTEXT_FLAGS_ARB, 0,
            0, 0
        };

        const char *glxExts = glXQueryExtensionsString(x11_display, DefaultScreen(x11_display));
        if (strstr(glxExts, "GLX_ARB_create_context") == 0) {
            std::cout << "GLX_ARB_create_context not supported"<<std::endl;
            m_context = glXCreateNewContext(x11_display, cfg.fb_config, GLX_RGBA_TYPE, 0, True);
        }
        else {
            m_context = glXCreateContextAttribsARB(x11_display, cfg.fb_config, 0, True, context_attribs);
        }

        XSync(x11_display, False);

        if (!m_context)
            throw std::runtime_error("Failed to create Gl Context.");

        // Verifying that context is a direct context
         if (!glXIsDirect (x11_display, m_context)) {
            Logger::debug("Indirect GLX rendering context obtained");
        }
        else {
            Logger::debug("Direct GLX rendering context obtained");
        }

        glXMakeContextCurrent(x11_display, 
                            cfg.m_buffer.win_id,
                            cfg.m_buffer.win_id,
                            m_context);

        GlSkiaContext::init(GrGLInterfaces::MakeGLX());

        Logger::debug("GL Renderer: %s, GL Version: %s, GLSL Version: %s", 
                    glGetString(GL_RENDERER), 
                    glGetString(GL_VERSION), 
                    glGetString(GL_SHADING_LANGUAGE_VERSION));
    }

    ContextGLX::~ContextGLX()
    {
        glXDestroyContext(x11_display, m_context);
    }

    void ContextGLX::makeCurrent(const Buffer &buf)
    {
        assert(x11_display);

        if (buf.win_id && m_context)
            glXMakeContextCurrent(x11_display, buf.win_id, buf.win_id, m_context);
        else
            glXMakeCurrent(x11_display, 0, nullptr);
    }

    void ContextGLX::flush(Buffer &buf)
    {
        assert(x11_display);

        if (m_context)
            glXSwapBuffers(x11_display, buf.win_id);
    }
}

struct Surface::Impl
{
    Impl(Display* display, Surface* holder, int width, int height):
        m_holder(holder),
        m_need_redraw(false),
        wm_delete_window_atom(XInternAtom(display, "WM_DELETE_WINDOW", False))
    {
        X11::ContextGLX::Config cfg(display, width, height);
        m_context.init(cfg);

        holder->skia_surf = m_context.makeSurface(width, height);
        m_buffer = cfg.move_buffer();

        XSetWMProtocols(display, m_buffer.win_id, &wm_delete_window_atom, 1);
        XClearWindow(display, m_buffer.win_id);
        XMapWindow(display, m_buffer.win_id);
    }

    ~Impl()
    {
        m_context.makeCurrent({});
        m_buffer.destroy(m_context.x11_display);
    }

    void event_process()
    {
        auto dpy = m_context.x11_display;

        if (m_need_redraw) draw();

        while(XPending(dpy)){

            XEvent xevent;
            XNextEvent(dpy, &xevent);

            /* input */
            if (xevent.type==KeyPress) {
                
            /* resize */
            } else if (xevent.type == ConfigureNotify
                    && !xevent.xconfigure.send_event) {

                m_holder->configure(xevent.xconfigure.width,
                                        xevent.xconfigure.height,
                                        SurfaceState::resizing);

            /* draw graphics */
            } else if (xevent.type          == Expose
                    && xevent.xexpose.count == 0 ) {
               
                if (xevent.xexpose.width == 1 && xevent.xexpose.height == 1) {
                    m_holder->configure(0, 0, SurfaceState::resizing);
                    continue;
                }
                
                draw();

            /* close window */
            } else if ( xevent.type                == ClientMessage
                    && xevent.xclient.data.l[0]    == wm_delete_window_atom ) {
                
                    m_holder->closed();
                   
            }else if ( xevent.type == MapNotify ) {
                std::cout << "MapNotify " <<std::endl;
            }
        }
    }

    void draw()
    {
        assert(m_holder);

        m_need_redraw = m_holder->draw(1);

        m_holder->skia_surf->flush();
        m_context.flush(m_buffer);
    }

    X11::ContextGLX m_context;
    X11::ContextGLX::Buffer m_buffer;
    Surface* m_holder;
    bool m_need_redraw;
    Atom wm_delete_window_atom;
};

class Application::DisplayImpl
{
public:
    static DisplayImpl& inst()
    {
        static DisplayImpl dpy;
        return dpy;
    }

    void start_event(bool &isrun);
    
    Display* display(){return m_display.get();}

private:
    DisplayImpl():
        syslogger("artist_lib_X11"),
        m_display(XOpenDisplay(NULL))
    {
        if (!m_display)
            throw std::runtime_error("Could not open display");
    }

    struct DpyDeleter
    {
        void operator()(Display* data) const
        {
            XCloseDisplay(data); 
        }
    };

    Logger syslogger;
    std::unique_ptr<Display, DpyDeleter> m_display;
    Surface::Impl* m_win{nullptr};
    friend class Surface;
};

void Application::DisplayImpl::start_event(bool &isrun)
{
    auto dpy = m_display.get();
    pollfd m_fds[2];

    m_fds[0].fd = ConnectionNumber(dpy);
    m_fds[0].events = POLLIN;
    m_fds[1].fd = -1; //To do add system interrupts
    m_fds[1].events = POLLIN;

    XFlush(dpy);

    while (isrun){

        int ret;
        do {
            ret = poll(m_fds, 1, -1);
        } while (ret < 0 && errno == EINTR);

        if (ret < 0) {
            if (m_fds[0].revents & POLLHUP)
                throw std::runtime_error("disconnected by display connection");

            throw std::runtime_error("failed to poll():");
        }

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
    impl(std::make_unique<Impl>(Application::DisplayImpl::inst().display(), this, width, height))
{
    Application::DisplayImpl::inst().m_win = impl.get();
}

void Surface::setTitle(const char* txt)
{
    XStoreName(impl->m_context.x11_display, impl->m_buffer.win_id, txt);
}

