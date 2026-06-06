/*=============================================================================
   Copyright (c) 2016-2023 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include "../../../app.hpp"
#include <gtk/gtk.h>
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
#include <algorithm>
#include <vector>

using namespace cycfi::artist;
float elapsed_ = 0;  // rendering elapsed time

namespace
{
   // Bench sizes: small → large → small
   static const std::vector<std::pair<int,int>> k_bench_sizes = {
      {200,150},{320,240},{480,320},{640,360},{640,480},
      {800,500},{1024,600},{1280,720},{1440,900},{1920,1080},
      {1920,1440},{1440,900},{1280,720},{1024,600},{800,500},
      {640,480},{640,360},{480,320},{320,240},{200,150}
   };
   static const int k_bench_passes = 6;

   struct view_state
   {
      extent   _size       = {};
      float    _scale      = 1.0f;
      int      _fb_w       = 0;
      int      _fb_h       = 0;
      bool     _animate    = false;
      bool     _resize_log = false;
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

      // Bench state
      bool                  _bench      = false;
      int                   _bench_step = 0;
      int                   _bench_pass = 0;
      std::vector<double>   _bench_times;
      GApplication*         _gapp       = nullptr;
      // resize start time — set just before gtk_window_resize
      std::chrono::steady_clock::time_point _resize_t0;
      bool                  _resize_pending = false;
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

      // Bench: record end-to-end resize+render time after a pending resize
      if (state._bench && state._resize_pending)
      {
         state._resize_pending = false;
         double ms = std::chrono::duration<double, std::milli>(
            stop - state._resize_t0).count();
         state._bench_times.push_back(ms);

         // Advance to next bench step
         state._bench_step++;
         if (state._bench_step >= int(k_bench_sizes.size()))
         {
            state._bench_step = 0;
            state._bench_pass++;
         }

         if (state._bench_pass >= k_bench_passes)
         {
            // Print results
            std::vector<double> sorted = state._bench_times;
            std::sort(sorted.begin(), sorted.end());
            double sum = 0; for (auto t : sorted) sum += t;
            size_t n = sorted.size();
            std::fprintf(stderr,
               "[bench] gtk3 skia   N=%zu"
               "  min=%.2f  med=%.2f  p95=%.2f  max=%.2f  mean=%.2f ms\n",
               n, sorted.front(), sorted[n/2],
               sorted[size_t(n*0.95)], sorted.back(), sum/n);
            g_application_quit(state._gapp);
         }
         else
         {
            // Trigger next resize
            auto [w, h] = k_bench_sizes[state._bench_step];
            state._resize_t0      = std::chrono::steady_clock::now();
            state._resize_pending = true;
            gtk_window_resize(state._window, w, h);
            gtk_widget_queue_draw(state._gl_area);
         }
      }

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
            if (state._resize_log)
               std::fprintf(stderr, "[resize] suppressed spurious half-size: physical=%dx%d\n",
                  width, height);
            return;
         }
      }
      state._primed = true;

      state._fb_w  = width;
      state._fb_h  = height;
      state._size  = {width / state._scale, height / state._scale};
      state._surface.reset();

      if (state._resize_log)
         std::fprintf(stderr, "[resize] physical=%dx%d  logical=%.0fx%.0f\n",
            width, height, state._size.x, state._size.y);
   }

   void activate(GtkApplication* app, gpointer user_data)
   {
      view_state& state = *reinterpret_cast<view_state*>(user_data);
      auto* window = gtk_application_window_new(app);
      state._window = GTK_WINDOW(window);
      state._gapp   = G_APPLICATION(app);
      gtk_window_set_title(GTK_WINDOW(window), "Artist (gtk3 skia)");

      g_signal_connect(window, "destroy", G_CALLBACK(close_window), user_data);

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

      // Prime away the spurious half-size GDK/XWayland first-resize event by
      // triggering a programmatic resize before the user can grab the handle.
      // The bad event fires and is silently suppressed in gl_resize.
      g_timeout_add(150, [](gpointer data) -> gboolean {
         view_state& st = *reinterpret_cast<view_state*>(data);
         gtk_window_resize(st._window, int(st._size.x) + 2, int(st._size.y));
         g_timeout_add(50, [](gpointer data2) -> gboolean {
            view_state& st2 = *reinterpret_cast<view_state*>(data2);
            gtk_window_resize(st2._window, int(st2._size.x), int(st2._size.y));
            return FALSE;
         }, data);
         return FALSE;
      }, user_data);

      if (std::getenv("ARTIST_RESIZE_BENCH"))
      {
         state._bench      = true;
         state._bench_step = 0;
         state._bench_pass = 0;
         // Kick off first resize after a short delay (let window settle first)
         g_timeout_add(200, [](gpointer data) -> gboolean {
            view_state& st = *reinterpret_cast<view_state*>(data);
            auto [w, h] = k_bench_sizes[st._bench_step];
            st._resize_t0      = std::chrono::steady_clock::now();
            st._resize_pending = true;
            gtk_window_resize(st._window, w, h);
            gtk_widget_queue_draw(st._gl_area);
            return FALSE;
         }, user_data);
      }
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
   state._animate    = animate;
   state._bkd        = background_color;
   state._resize_log = std::getenv("ARTIST_RESIZE_LOG") != nullptr;

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
