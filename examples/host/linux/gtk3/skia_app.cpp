/*=============================================================================
   Copyright (c) 2016-2023 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include "../../../app.hpp"
#include <gtk/gtk.h>
#include <gdk/gdkx.h>
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
#include <cstdlib>
#include <cstdio>

using namespace cycfi::artist;
float elapsed_ = 0;  // rendering elapsed time

namespace
{
   struct view_state
   {
      extent   _size       = {};
      extent   _requested  = {};
      extent   _prime_size = {};
      float    _scale      = 1.0f;
      int      _fb_w       = 0;
      int      _fb_h       = 0;
      bool     _animate    = false;
      color    _bkd        = colors::white;
      guint    _timer_id   = 0;

      GtkWidget*   _gl_area = nullptr;
      GtkWindow*   _window  = nullptr;

      sk_sp<const GrGLInterface> _xface;
      sk_sp<GrDirectContext>     _ctx;
      sk_sp<SkSurface>           _surface;

      // First-resize guard: GDK/XWayland emits a spurious half-size gl_resize
      // on the very first user resize. We prime it away at startup.
      bool                  _primed     = false;

      // ARTIST_PERF: an EGL context and surface on the toplevel window's own
      // X11 window, so frames are swapped straight to the window instead of
      // being composited by GTK in its paint cycle.
      EGLDisplay   _egl_display = EGL_NO_DISPLAY;
      EGLConfig    _egl_config  = nullptr;
      EGLContext   _egl_context = EGL_NO_CONTEXT;
      EGLSurface   _egl_surface = EGL_NO_SURFACE;
      int          _stencil     = 0;
      int          _perf_w      = 0;
      int          _perf_h      = 0;
   };

   void close_window(GtkWidget*, gpointer user_data)
   {
      view_state& state = *reinterpret_cast<view_state*>(user_data);
      if (state._timer_id)
         g_source_remove(state._timer_id);
   }

   void realize(GtkGLArea* area, gpointer user_data)
   {
      auto error = [](char const* msg) { throw std::runtime_error(msg); };
      gtk_gl_area_make_current(area);
      if (gtk_gl_area_get_error(area) != nullptr)
         error("Error: gtk_gl_area_get_error failed");

      view_state& state = *reinterpret_cast<view_state*>(user_data);
      glClearColor(state._bkd.red, state._bkd.green, state._bkd.blue, state._bkd.alpha);
      glClear(GL_COLOR_BUFFER_BIT);
      if (state._xface = GrGLMakeNativeInterface(); state._xface == nullptr)
      {
         state._xface = GrGLInterfaces::MakeEGL();
         if (state._xface == nullptr)
            error("Error: GrGLMakeNativeInterface failed");
      }
      if (state._ctx = GrDirectContexts::MakeGL(state._xface); state._ctx == nullptr)
         error("Error: GrDirectContexts::MakeGL failed");
   }

   gboolean render(GtkGLArea* /*area*/, GdkGLContext* /*context*/, gpointer user_data)
   {
      view_state& state = *reinterpret_cast<view_state*>(user_data);
      auto error = [](char const* msg) { throw std::runtime_error(msg); };

      auto start = std::chrono::steady_clock::now();

      if (!state._surface)
      {
         GrGLint buffer;
         glGetIntegerv(GL_FRAMEBUFFER_BINDING, &buffer);
         GrGLFramebufferInfo info;
         info.fFBOID  = (GrGLuint)buffer;
         info.fFormat = GL_RGBA8;

         auto target = GrBackendRenderTargets::MakeGL(
            state._fb_w, state._fb_h, 0, 8, info);

         state._surface = SkSurfaces::WrapBackendRenderTarget(
            state._ctx.get(), target,
            kBottomLeft_GrSurfaceOrigin, kRGBA_8888_SkColorType,
            nullptr, nullptr);

         if (!state._surface)
            error("Error: SkSurfaces::WrapBackendRenderTarget returned null");
      }

      SkCanvas* gpu_canvas = state._surface->getCanvas();
      gpu_canvas->save();
      gpu_canvas->scale(state._scale, state._scale);
      auto cnv = canvas{gpu_canvas};
      draw(cnv);
      gpu_canvas->restore();
      state._ctx->flushAndSubmit(state._surface.get());

      auto stop = std::chrono::steady_clock::now();
      elapsed_ = std::chrono::duration<double>{stop - start}.count();

      return true;
   }

   gboolean animate_cb(gpointer user_data)
   {
      gtk_widget_queue_draw(GTK_WIDGET(user_data));
      return true;
   }

   void gl_resize(GtkGLArea*, gint width, gint height, gpointer user_data)
   {
      view_state& state = *reinterpret_cast<view_state*>(user_data);

      // Suppress the spurious half-size event that GDK/XWayland emits on the
      // first resize. We prime it away at startup; this guard makes that
      // programmatic prime invisible (no surface reset, no visual glitch).
      if (!state._primed && state._fb_w > 0)
      {
         if (float(width) / float(state._fb_w) < 0.6f)
         {
            state._primed = true;
            return;
         }
      }
      state._primed = true;

      state._fb_w  = width;
      state._fb_h  = height;
      state._size  = {width / state._scale, height / state._scale};
      state._surface.reset();
   }

   bool perf_fail(char const* what)
   {
      g_printerr("Error: ARTIST_PERF EGL setup: %s (EGL error 0x%x)\n",
         what, unsigned(eglGetError()));
      return false;
   }

   // Choose the EGL config first and give the window that config's X visual,
   // as the x11 host does. A window surface needs the config's own visual; the
   // one GTK picks for the window has no EGL config.
   bool perf_choose_config(view_state& state, GtkWidget* window)
   {
      auto* xdisplay = gdk_x11_display_get_xdisplay(gtk_widget_get_display(window));
      state._egl_display = eglGetDisplay((EGLNativeDisplayType) xdisplay);
      if (state._egl_display == EGL_NO_DISPLAY)
         return perf_fail("eglGetDisplay");
      if (!eglInitialize(state._egl_display, nullptr, nullptr))
         return perf_fail("eglInitialize");

      EGLint const attribs[] = {
         EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
         EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
         EGL_RED_SIZE,   8,
         EGL_GREEN_SIZE, 8,
         EGL_BLUE_SIZE,  8,
         EGL_ALPHA_SIZE, 8,
         EGL_NONE
      };
      EGLint count = 0;
      if (!eglChooseConfig(state._egl_display, attribs, &state._egl_config, 1, &count)
         || count == 0)
         return perf_fail("eglChooseConfig");
      eglGetConfigAttrib(state._egl_display, state._egl_config, EGL_STENCIL_SIZE, &state._stencil);

      EGLint visual_id = 0;
      eglGetConfigAttrib(state._egl_display, state._egl_config, EGL_NATIVE_VISUAL_ID, &visual_id);
      auto* visual = gdk_x11_screen_lookup_visual(
         GDK_X11_SCREEN(gtk_widget_get_screen(window)), VisualID(visual_id));
      if (!visual)
         return perf_fail("no GDK visual for the EGL config");
      gtk_widget_set_visual(window, visual);
      return true;
   }

   bool perf_init(view_state& state)
   {
      auto* win = gtk_widget_get_window(GTK_WIDGET(state._window));
      if (!win)
         return perf_fail("the window has no GdkWindow");

      eglBindAPI(EGL_OPENGL_API);
      EGLint ctx_attribs[] = { EGL_NONE };
      state._egl_context = eglCreateContext(
         state._egl_display, state._egl_config, EGL_NO_CONTEXT, ctx_attribs);
      if (state._egl_context == EGL_NO_CONTEXT)
         return perf_fail("eglCreateContext");
      state._egl_surface = eglCreateWindowSurface(
         state._egl_display, state._egl_config,
         (EGLNativeWindowType) gdk_x11_window_get_xid(win), nullptr);
      if (state._egl_surface == EGL_NO_SURFACE)
         return perf_fail("eglCreateWindowSurface");
      if (!eglMakeCurrent(state._egl_display, state._egl_surface,
                          state._egl_surface, state._egl_context))
         return perf_fail("eglMakeCurrent");

      // The swap must not wait for the vblank.
      eglSwapInterval(state._egl_display, 0);

      state._xface = GrGLMakeNativeInterface();
      if (!state._xface)
         state._xface = GrGLInterfaces::MakeEGL();
      if (!state._xface)
         return perf_fail("Skia GL interface");
      state._ctx = GrDirectContexts::MakeGL(state._xface);
      if (!state._ctx)
         return perf_fail("Skia GL context");
      return true;
   }

   // ARTIST_PERF frames: draw, wait for the GPU, and swap straight to the
   // window. The frame time runs from the start of the drawing to the swap.
   gboolean perf_frame(gpointer user_data)
   {
      view_state& state = *reinterpret_cast<view_state*>(user_data);
      if (!state._ctx && !perf_init(state))
         std::exit(1);

      auto* widget = GTK_WIDGET(state._window);
      int const scale = gdk_window_get_scale_factor(gtk_widget_get_window(widget));
      int const w = gtk_widget_get_allocated_width(widget) * scale;
      int const h = gtk_widget_get_allocated_height(widget) * scale;
      if (w <= 0 || h <= 0)
         return G_SOURCE_CONTINUE;

      if (!state._surface || w != state._perf_w || h != state._perf_h)
      {
         state._surface.reset();
         GrGLFramebufferInfo info;
         info.fFBOID  = 0;
         info.fFormat = GL_RGBA8;
         auto target = GrBackendRenderTargets::MakeGL(w, h, 0, state._stencil, info);
         state._surface = SkSurfaces::WrapBackendRenderTarget(
            state._ctx.get(), target,
            kBottomLeft_GrSurfaceOrigin, kRGBA_8888_SkColorType,
            nullptr, nullptr);
         state._perf_w = w;
         state._perf_h = h;
         if (!state._surface)
         {
            g_printerr("Error: SkSurfaces::WrapBackendRenderTarget returned null\n");
            std::exit(1);
         }
      }

      auto start = std::chrono::steady_clock::now();
      SkCanvas* gpu_canvas = state._surface->getCanvas();
      gpu_canvas->save();
      gpu_canvas->scale(scale, scale);
      {
         auto cnv = canvas{gpu_canvas};
         draw(cnv);
      }
      gpu_canvas->restore();
      state._ctx->flushAndSubmit(state._surface.get(), GrSyncCpu::kYes);
      eglSwapBuffers(state._egl_display, state._egl_surface);
      auto stop = std::chrono::steady_clock::now();

      elapsed_ = std::chrono::duration<double>{stop - start}.count();
      perf_record(elapsed_, w, h);
      return G_SOURCE_CONTINUE;
   }

   // ARTIST_PERF: start frames only once the window has the requested size,
   // so every recorded frame has that size. Ask again while the window
   // manager has not applied it.
   gboolean perf_wait_for_size(gpointer user_data)
   {
      view_state& state = *reinterpret_cast<view_state*>(user_data);
      auto* widget = GTK_WIDGET(state._window);
      int const w = gtk_widget_get_allocated_width(widget);
      int const h = gtk_widget_get_allocated_height(widget);
      if (w == int(state._requested.x) && h == int(state._requested.y))
      {
         state._timer_id = g_idle_add(perf_frame, user_data);
         return G_SOURCE_REMOVE;
      }
      gtk_window_resize(state._window, int(state._requested.x), int(state._requested.y));
      return G_SOURCE_CONTINUE;
   }

   // Prime away the spurious half-size GDK/XWayland first-resize event by
   // triggering a programmatic resize before the user can grab the handle.
   // The bad event fires and is silently suppressed in gl_resize. The restore
   // uses the size from before the prime, since resize events may already
   // have moved _size to the primed width.
   void prime_resize(gpointer user_data)
   {
      g_timeout_add(150, [](gpointer data) -> gboolean {
         view_state& st = *reinterpret_cast<view_state*>(data);
         st._prime_size = st._size;
         gtk_window_resize(st._window, int(st._size.x) + 2, int(st._size.y));
         g_timeout_add(50, [](gpointer data2) -> gboolean {
            view_state& st2 = *reinterpret_cast<view_state*>(data2);
            gtk_window_resize(st2._window,
               int(st2._prime_size.x), int(st2._prime_size.y));
            return FALSE;
         }, data);
         return FALSE;
      }, user_data);
   }

   void activate(GtkApplication* app, gpointer user_data)
   {
      view_state& state = *reinterpret_cast<view_state*>(user_data);
      auto* window = gtk_application_window_new(app);
      state._window = GTK_WINDOW(window);
      gtk_window_set_title(GTK_WINDOW(window), "Artist (gtk3 skia)");

      g_signal_connect(window, "destroy", G_CALLBACK(close_window), user_data);

      if (perf_enabled())
      {
         // Measuring draws with EGL straight into the toplevel's X11 window,
         // which GDK's X11 backend decorates server side, so the window is
         // the content area. GTK must not paint over the frames. Measuring is
         // unattended, so the first-resize prime is not needed; frames wait
         // for the requested size instead.
         if (!perf_choose_config(state, window))
            std::exit(1);
         gtk_widget_set_app_paintable(window, TRUE);
         gtk_window_resize(GTK_WINDOW(window), state._size.x, state._size.y);
         gtk_widget_show_all(window);
         g_timeout_add(100, perf_wait_for_size, user_data);
         return;
      }

      GtkWidget* gl_area = gtk_gl_area_new();
      state._gl_area = gl_area;
      gtk_container_add(GTK_CONTAINER(window), gl_area);

      g_signal_connect(gl_area, "render",  G_CALLBACK(render),    user_data);
      g_signal_connect(gl_area, "realize", G_CALLBACK(realize),   user_data);
      g_signal_connect(gl_area, "resize",  G_CALLBACK(gl_resize), user_data);

      gtk_window_resize(GTK_WINDOW(window), state._size.x, state._size.y);
      gtk_widget_show_all(window);

      auto w = gtk_widget_get_window(GTK_WIDGET(window));
      state._scale = float(gdk_window_get_scale_factor(w));

      // gl_resize fired during show_all before _scale was known; recompute
      // logical size now that we have the correct scale factor.
      if (state._fb_w > 0)
         state._size = {state._fb_w / state._scale, state._fb_h / state._scale};

      if (state._animate)
         state._timer_id = g_timeout_add(1000/60, animate_cb, gl_area);

      prime_resize(user_data);
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
   int argc
 , char const* argv[]
 , extent window_size
 , color background_color
 , bool animate
)
{
   view_state state;
   state._size       = window_size;
   state._requested  = window_size;
   state._animate    = animate;
   state._bkd        = background_color;

   // Measuring swaps frames straight to the window's X11 window with EGL,
   // which needs GDK's X11 backend.
   if (perf_enabled())
      gdk_set_allowed_backends("x11");

   auto* app = gtk_application_new("org.cycfi.artist.gtk3skia",
                                   G_APPLICATION_DEFAULT_FLAGS);
   int status = 0;
   try
   {
      g_signal_connect(app, "activate", G_CALLBACK(activate), &state);
      status = g_application_run(G_APPLICATION(app), argc, const_cast<char**>(argv));
   }
   catch (std::runtime_error const& e)
   {
      g_printerr("%s\n", e.what());
      status = 1;
   }
   g_object_unref(app);
   return status;
}
