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

using namespace cycfi::artist;
float elapsed_ = 0;  // rendering elapsed time

namespace
{
   struct view_state
   {
      extent   _size       = {};
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

   void activate(GtkApplication* app, gpointer user_data)
   {
      view_state& state = *reinterpret_cast<view_state*>(user_data);
      auto* window = gtk_application_window_new(app);
      state._window = GTK_WINDOW(window);
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
