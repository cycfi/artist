/*=============================================================================
   Copyright (c) 2016-2023 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include "../../app.hpp"
//#include <gtk/gtk.h>
#include <GL/gl.h>
//#include <GL/glx.h>

#include "GrDirectContext.h"
#include "gl/GrGLInterface.h"
#include "gl/GrGLAssembleInterface.h"
//#include "SkImage.h"
#include "SkColorSpace.h"
#include "SkCanvas.h"
#include "SkSurface.h"
#include <chrono>

#include "wayland/contextegl.h"
#include "wayland/xdgshell.h"

using namespace cycfi::artist;
float elapsed_ = 0;  // rendering elapsed time

void render(SkCanvas* gpu_canvas, float scale)
{
    auto start = std::chrono::steady_clock::now();

    gpu_canvas->save();
    gpu_canvas->scale(scale, scale);
    auto cnv = canvas{gpu_canvas};
    draw(cnv);
    gpu_canvas->restore();

    auto stop = std::chrono::steady_clock::now();
    elapsed_ = std::chrono::duration<double>{stop - start}.count();
}

namespace cycfi::artist
{
   void init_paths()
   {
      add_search_path(fs::current_path() / "resources");
      add_search_path(fs::current_path() / "resources/fonts");
      add_search_path(fs::current_path() / "resources/images");
   }

   // This is declared in font.hpp
   fs::path get_user_fonts_directory()
   {
      return fs::path(fs::current_path() / "resources/fonts");
   }
}

namespace WL {

class PaintContextSkia;

class SurfaceSkia: public ContextEGL::BufferEGL
{
public:
    SurfaceSkia(PaintContextSkia &ctx, NativeSurface &wl, uint32_t width, uint32_t height):
       ContextEGL::BufferEGL(ctx, wl, width, height)
   {
       if (ContextEGL::BufferEGL::valid()){
   
           GrGLFramebufferInfo framebufferInfo;
           framebufferInfo.fFBOID = 0; // assume default framebuffer
           framebufferInfo.fFormat = GL_RGBA8;
   
           SkColorType colorType = kRGBA_8888_SkColorType;
           GrBackendRenderTarget target(width,
                                        height,
                                        1, // sample count
                                        8, // stencil bits
                                        framebufferInfo);
   
           skia = SkSurface::MakeFromBackendRenderTarget(ctx._ctx.get(),
                                                                target,
                                                                kBottomLeft_GrSurfaceOrigin,
                                                                colorType,
                                                                nullptr,
                                                                nullptr);
           if (!skia) ctx.destroy(*this);
       }
   }

    sk_sp<SkSurface> skia;
};

class PaintContextSkia : public ContextEGL
{
public:
    using Buffer = SurfaceSkia;

    void flush(Buffer &buf) const
    {
       buf.skia->flush();
       ContextEGL::flush(buf);
    }
    void destroy(Buffer &buf)
    {
       buf.skia.reset();
       ContextEGL::destroy(buf);
    }

    PaintContextSkia(bool transparent = false):
       ContextEGL(transparent)
   {
       auto _xface = GrGLMakeNativeInterface();
       if (_xface == nullptr) {
           //backup plan. see https://gist.github.com/ad8e/dd150b775ae6aa4d5cf1a092e4713add?permalink_comment_id=4680136#gistcomment-4680136
           _xface = GrGLMakeAssembledInterface(
               nullptr, (GrGLGetProc) * [](void *, const char *p) -> void * {
                   return (void *) eglGetProcAddress(p);
               });
       }
   
       _ctx = GrDirectContext::MakeGL(_xface);
       if (!_ctx)
           throw std::runtime_error("failed to make Skia context");
   }

private:
    sk_sp<GrDirectContext> _ctx;
    friend class SurfaceSkia;
};

class Window: public Toplevel<PaintContextSkia>,
              public SeatListener
{
public:
    Window(int width, int height):
        Toplevel<PaintContextSkia>(width, height)//,
        //m_opaque(ctx.opaque())
    {
        listenerInput(*this);
    }

    std::function<void()> onClosed;
    std::function<void(SkCanvas *surf, float scale)> onDraw;
    std::function<void(int,int)> onReshape;

private:
    //bool m_opaque;

    void draw(float scale) override
    {if (onDraw) onDraw(m_buffer->skia->getCanvas(), scale);}
    void closed() override
    {if (onClosed) onClosed();}
    bool configure(uint32_t width, uint32_t height, uint32_t state) override
    {
       if (onReshape && state & WL::XDGState::resizing) {
           onReshape(width, height);
           return true;
       }
       return false;
    }
};

}

int run_app(
   int argc
 , char const* argv[]
 , extent window_size
 , color background_color
 , bool animate
)
{
   try {

        auto& dpy = WL::Display::init<WL::XDGWmBase, WL::XDGDecorateManager>();

        WL::Window window(window_size.x, window_size.y);

        window.onClosed = [&dpy](){dpy.stop();};
        window.setTitle("Example application");
        window.onDraw = [&window, animate](SkCanvas *surf, float scale ) {
            render(surf, scale);

            if (animate)
                window.refresh();
        };
        window.onReshape = [&background_color](int, int) {
            glClearColor(background_color.red + 0.1,
                         background_color.green,
                         background_color.blue,
                         background_color.alpha);
            glClear(GL_COLOR_BUFFER_BIT);
        };

        dpy.start_event();

    } catch (const char* err) {
        std::cerr<<"Error: "<<err<<std::endl;
        exit(1);
    }

    return 0;
}


