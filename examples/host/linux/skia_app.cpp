/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include "../../app.hpp"
#include <gtk/gtk.h>
#include <GL/gl.h>
#include <GL/glx.h>

#include "GrContext.h"
#include "gl/GrGLInterface.h"
#include "SkImage.h"
#include "SkSurface.h"
#include <chrono>

using namespace cycfi::artist;
float elapsed_ = 0;  // rendering elapsed time

namespace
{
   extent   _size = {};
   float    _scale = 1.0;
   bool     _animate = false;
   color    _bkd = colors::white;
   bool     _first_time = true;
   guint    _timer_id = 0;

   void close_window()
   {
      if (_timer_id)
         g_source_remove(_timer_id);
   }

   gboolean render(GtkGLArea* area, GdkGLContext* context)
   {
      auto start = std::chrono::steady_clock::now();
      auto error = [](char const* msg) { throw std::runtime_error(msg); };

      if (_first_time)
      {
         glClearColor(_bkd.red, _bkd.green, _bkd.blue, _bkd.alpha);
         glClear(GL_COLOR_BUFFER_BIT);
         _first_time = false;
      }

      auto xface = GrGLMakeNativeInterface();
      sk_sp<GrContext> ctx = GrContext::MakeGL(xface);

      GrGLint buffer;
      glGetIntegerv(GL_FRAMEBUFFER_BINDING, &buffer);
      GrGLFramebufferInfo info;
      info.fFBOID = (GrGLuint) buffer;
      SkColorType colorType = kRGBA_8888_SkColorType;

      info.fFormat = GL_RGBA8;
      GrBackendRenderTarget target(_size.x*_scale, _size.y*_scale, 0, 8, info);

      sk_sp<SkSurface> surface(
         SkSurface::MakeFromBackendRenderTarget(ctx.get(), target,
         kBottomLeft_GrSurfaceOrigin, colorType, nullptr, nullptr));

      if (!surface)
         error("Error: SkSurface::MakeRenderTarget returned null");

      SkCanvas* gpu_canvas = surface->getCanvas();
      auto cnv = canvas{ gpu_canvas };
      cnv.pre_scale({ _scale, _scale });
      draw(cnv);

      auto stop = std::chrono::steady_clock::now();
      elapsed_ = std::chrono::duration<double>{ stop - start }.count();

      return true;
   }

   gboolean animate(gpointer user_data)
   {
      GtkWidget* da = GTK_WIDGET(user_data);
      gtk_widget_queue_draw(da);
      return true;
   }

   // $$$ TODO: Investigate $$$
   // Somehow, this prevents us from having linker errors
   // Without this, we get undefined reference to `glXGetCurrentContext'
   auto proc = &glXGetProcAddress;

   void activate(GtkApplication* app, gpointer user_data)
   {
      auto error = [](char const* msg) { throw std::runtime_error(msg); };
      if (!proc)
         error("Error: glXGetProcAddress is null");

      extent const& p = *reinterpret_cast<extent const*>(user_data);
      auto* window = gtk_application_window_new(app);
      gtk_window_set_title(GTK_WINDOW(window), "Drawing Area");

      g_signal_connect(window, "destroy", G_CALLBACK(close_window), nullptr);

      // create a GtkGLArea instance
      auto* gl_area = gtk_gl_area_new();
      gtk_container_add(GTK_CONTAINER(window), gl_area);

      g_signal_connect(gl_area, "render", G_CALLBACK(render), nullptr);

      gtk_window_resize(GTK_WINDOW(window), p.x, p.y);
      gtk_widget_show_all(window);

      auto w = gtk_widget_get_window(GTK_WIDGET(window));
      _scale = gdk_window_get_scale_factor(w);

      if (_animate)
         _timer_id = g_timeout_add(1000 / 60, animate, gl_area);
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
}

int run_app(
   int argc
 , char const* argv[]
 , extent window_size
 , color background_color
 , bool animate
)
{
   _size = window_size;
   _animate = animate;
   _bkd = background_color;

   auto* app = gtk_application_new("org.gtk-skia.example", G_APPLICATION_FLAGS_NONE);
   g_signal_connect(app, "activate", G_CALLBACK(activate), &window_size);
   int status = g_application_run(G_APPLICATION(app), argc, const_cast<char**>(argv));
   g_object_unref(app);

   return status;
}



