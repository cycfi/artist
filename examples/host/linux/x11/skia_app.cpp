/*=============================================================================
   Copyright (c) 2016-2023 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include "../../../app.hpp"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xresource.h>
#include <EGL/egl.h>
#include <GL/gl.h>

#include <SkImage.h>
#include <SkColorSpace.h>
#include <SkCanvas.h>
#include <SkSurface.h>
#include <ganesh/GrDirectContext.h>
#include <ganesh/GrBackendSurface.h>
#include <ganesh/SkSurfaceGanesh.h>
#include <ganesh/gl/GrGLInterface.h>
#include <ganesh/gl/GrGLDirectContext.h>
#include <ganesh/gl/GrGLBackendSurface.h>
#include <ganesh/gl/GrGLTypes.h>
#include <ganesh/gl/egl/GrGLMakeEGLInterface.h>

#include <chrono>
#include <thread>
#include <cmath>
#include <cstdlib>
#include <stdexcept>

using namespace cycfi::artist;
float elapsed_ = 0;  // rendering elapsed time

namespace
{
   struct app_state
   {
      Display*      display = nullptr;
      int           screen  = 0;
      Window        window  = 0;
      Atom          wm_delete = 0;

      EGLDisplay    egl_display = EGL_NO_DISPLAY;
      EGLContext    egl_context = EGL_NO_CONTEXT;
      EGLSurface    egl_surface = EGL_NO_SURFACE;

      sk_sp<const GrGLInterface> xface;
      sk_sp<GrDirectContext>     ctx;
      sk_sp<SkSurface>           skia_surface;

      extent        size    = {};
      double        scale   = 1.0;
      color         bkd     = colors::white;
      bool          animate = false;
      bool          running = true;
   };

   // -------------------------------------------------------------------------
   // HiDPI scale from the X resource manager (Xft.dpi / 96)
   double get_scale(Display* display)
   {
      double scale = 1.0;
      if (char* rms = XResourceManagerString(display))
      {
         if (XrmDatabase db = XrmGetStringDatabase(rms))
         {
            XrmValue value;
            char* type = nullptr;
            if (XrmGetResource(db, "Xft.dpi", "Xft.Dpi", &type, &value) && value.addr)
            {
               if (double dpi = std::atof(value.addr); dpi > 0)
                  scale = dpi / 96.0;
            }
            XrmDestroyDatabase(db);
         }
      }
      return scale;
   }

   // -------------------------------------------------------------------------
   // Create (or recreate) the Skia render surface at current physical size
   void create_skia_surface(app_state& state)
   {
      state.skia_surface.reset();

      int const w = int(std::lround(state.size.x * state.scale));
      int const h = int(std::lround(state.size.y * state.scale));

      GrGLint buffer = 0;
      glGetIntegerv(GL_FRAMEBUFFER_BINDING, &buffer);
      GrGLFramebufferInfo info;
      info.fFBOID  = (GrGLuint)buffer;
      info.fFormat = GL_RGBA8;

      auto target = GrBackendRenderTargets::MakeGL(w, h, 0, 8, info);
      state.skia_surface = SkSurfaces::WrapBackendRenderTarget(
         state.ctx.get(), target,
         kBottomLeft_GrSurfaceOrigin, kRGBA_8888_SkColorType,
         nullptr, nullptr
      );
      if (!state.skia_surface)
         throw std::runtime_error("SkSurfaces::WrapBackendRenderTarget failed");
   }

   // -------------------------------------------------------------------------
   // Render via Skia Ganesh GL
   void render(app_state& state)
   {
      if (!state.skia_surface)
         return;

      auto start = std::chrono::steady_clock::now();

      SkCanvas* gpu_canvas = state.skia_surface->getCanvas();
      gpu_canvas->save();
      gpu_canvas->scale(state.scale, state.scale);
      auto cnv = canvas{gpu_canvas};
      draw(cnv);
      gpu_canvas->restore();
      state.ctx->flushAndSubmit(state.skia_surface.get());

      auto stop = std::chrono::steady_clock::now();
      elapsed_ = std::chrono::duration<double>{stop - start}.count();

      eglSwapBuffers(state.egl_display, state.egl_surface);
   }

   // -------------------------------------------------------------------------
   // Dispatch a single X11 event
   void dispatch(app_state& state, XEvent& ev)
   {
      switch (ev.type)
      {
         case Expose:
            if (ev.xexpose.count == 0)
               render(state);
            break;

         case ConfigureNotify:
            // Fixed-size window; recreate surface if the size somehow changed.
            create_skia_surface(state);
            break;

         case ClientMessage:
            if (Atom(ev.xclient.data.l[0]) == state.wm_delete)
               state.running = false;
            break;

         default:
            break;
      }
   }

   // -------------------------------------------------------------------------
   // EGL init + an X11 window whose visual matches the chosen EGL config
   void init_egl_and_window(app_state& state, int w, int h)
   {
      state.egl_display = eglGetDisplay((EGLNativeDisplayType)state.display);
      if (state.egl_display == EGL_NO_DISPLAY)
         throw std::runtime_error("eglGetDisplay failed");
      if (!eglInitialize(state.egl_display, nullptr, nullptr))
         throw std::runtime_error("eglInitialize failed");

      EGLint config_attribs[] = {
         EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
         EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
         EGL_RED_SIZE,   8,
         EGL_GREEN_SIZE, 8,
         EGL_BLUE_SIZE,  8,
         EGL_ALPHA_SIZE, 8,
         EGL_NONE
      };
      EGLConfig config = nullptr;
      EGLint    num_configs = 0;
      if (!eglChooseConfig(state.egl_display, config_attribs, &config, 1, &num_configs)
            || num_configs == 0)
         throw std::runtime_error("eglChooseConfig failed");

      // The X11 window must use the visual the EGL config expects, else BadMatch.
      EGLint visual_id = 0;
      eglGetConfigAttrib(state.egl_display, config, EGL_NATIVE_VISUAL_ID, &visual_id);

      XVisualInfo vtemplate;
      vtemplate.visualid = VisualID(visual_id);
      int nvis = 0;
      XVisualInfo* vinfo = XGetVisualInfo(state.display, VisualIDMask, &vtemplate, &nvis);
      if (!vinfo || nvis == 0)
         throw std::runtime_error("No X visual matching the EGL config");

      Window root = RootWindow(state.display, state.screen);
      Colormap cmap = XCreateColormap(state.display, root, vinfo->visual, AllocNone);

      XSetWindowAttributes swa = {};
      swa.colormap      = cmap;
      swa.background_pixel = 0;
      swa.border_pixel  = 0;
      swa.event_mask    = ExposureMask | StructureNotifyMask;

      state.window = XCreateWindow(
         state.display, root,
         0, 0, w, h, 0,
         vinfo->depth, InputOutput, vinfo->visual,
         CWColormap | CWEventMask | CWBackPixel | CWBorderPixel, &swa
      );
      XFree(vinfo);

      eglBindAPI(EGL_OPENGL_API);
      EGLint ctx_attribs[] = { EGL_NONE };
      state.egl_context = eglCreateContext(
         state.egl_display, config, EGL_NO_CONTEXT, ctx_attribs);
      if (state.egl_context == EGL_NO_CONTEXT)
         throw std::runtime_error("eglCreateContext failed");

      state.egl_surface = eglCreateWindowSurface(
         state.egl_display, config, (EGLNativeWindowType)state.window, nullptr);
      if (state.egl_surface == EGL_NO_SURFACE)
         throw std::runtime_error("eglCreateWindowSurface failed");

      if (!eglMakeCurrent(state.egl_display, state.egl_surface,
                          state.egl_surface, state.egl_context))
         throw std::runtime_error("eglMakeCurrent failed");
   }

   // -------------------------------------------------------------------------
   void init_skia(app_state& state)
   {
      state.xface = GrGLMakeNativeInterface();
      if (!state.xface)
         state.xface = GrGLInterfaces::MakeEGL();
      if (!state.xface)
         throw std::runtime_error("GrGLMakeNativeInterface / MakeEGL failed");
      state.ctx = GrDirectContexts::MakeGL(state.xface);
      if (!state.ctx)
         throw std::runtime_error("GrDirectContexts::MakeGL failed");
   }
}

namespace cycfi::artist
{
   void init_paths()
   {
      add_search_path(fs::current_path() / "resources");
      add_search_path(fs::current_path() / "resources/fonts");
      add_search_path(fs::current_path() / "resources/images");
   }

   fs::path get_user_fonts_directory()
   {
      return fs::path(fs::current_path() / "resources/fonts");
   }
}

int run_app(
   int         /*argc*/
 , char const* /*argv*/[]
 , extent      window_size
 , color       background_color
 , bool        animate
)
{
   app_state state;
   state.size    = window_size;
   state.bkd     = background_color;
   state.animate = animate;

   state.display = XOpenDisplay(nullptr);
   if (!state.display)
      throw std::runtime_error("Failed to open X display");

   state.screen = DefaultScreen(state.display);
   state.scale  = get_scale(state.display);

   int const w = int(std::lround(state.size.x * state.scale));
   int const h = int(std::lround(state.size.y * state.scale));

   init_egl_and_window(state, w, h);

   // Window title
   XStoreName(state.display, state.window, "Artist");

   // Fixed-size window: pin min == max
   if (XSizeHints* hints = XAllocSizeHints())
   {
      hints->flags      = PMinSize | PMaxSize;
      hints->min_width  = hints->max_width  = w;
      hints->min_height = hints->max_height = h;
      XSetWMNormalHints(state.display, state.window, hints);
      XFree(hints);
   }

   state.wm_delete = XInternAtom(state.display, "WM_DELETE_WINDOW", False);
   XSetWMProtocols(state.display, state.window, &state.wm_delete, 1);

   XMapWindow(state.display, state.window);

   init_skia(state);
   create_skia_surface(state);

   // Event loop
   using clock = std::chrono::steady_clock;
   auto next_frame = clock::now();
   constexpr auto frame_interval = std::chrono::milliseconds(1000 / 60);

   while (state.running)
   {
      if (state.animate)
      {
         while (XPending(state.display))
         {
            XEvent ev;
            XNextEvent(state.display, &ev);
            dispatch(state, ev);
         }
         render(state);
         next_frame += frame_interval;
         std::this_thread::sleep_until(next_frame);
      }
      else
      {
         XEvent ev;
         XNextEvent(state.display, &ev);
         dispatch(state, ev);
      }
   }

   // Tear down
   state.skia_surface.reset();
   state.ctx.reset();
   state.xface.reset();
   if (state.egl_surface != EGL_NO_SURFACE)
      eglDestroySurface(state.egl_display, state.egl_surface);
   if (state.egl_context != EGL_NO_CONTEXT)
      eglDestroyContext(state.egl_display, state.egl_context);
   if (state.egl_display != EGL_NO_DISPLAY)
      eglTerminate(state.egl_display);
   if (state.window)
      XDestroyWindow(state.display, state.window);
   XCloseDisplay(state.display);
   return 0;
}
