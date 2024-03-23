/*=============================================================================
   Copyright (c) 2016-2023 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include "../../app.hpp"
#include <gtk/gtk.h>
#include <GL/gl.h>
#include <GL/glx.h>

#include "GrDirectContext.h"
#include "gl/GrGLInterface.h"
#include "SkImage.h"
#include "SkColorSpace.h"
#include "SkCanvas.h"
#include "SkSurface.h"
#include <chrono>

using namespace cycfi::artist;
float elapsed_ = 0;  // rendering elapsed time

namespace
{
   struct view_state
   {
      extent   _size = {};
      float    _scale = 1.0;
      bool     _animate = false;
      color    _bkd = colors::white;
      guint    _timer_id = 0;

      sk_sp<const GrGLInterface> _xface;
      sk_sp<GrDirectContext>     _ctx;
      sk_sp<SkSurface>           _surface;
   };

   void close_window(GtkWidget*, gpointer user_data)
   {
      view_state& state = *reinterpret_cast<view_state*>(user_data);
      if (state._timer_id)
         g_source_remove(state._timer_id);
   }

   void realize(GtkGLArea* area, gpointer user_data)
   {
      gtk_gl_area_make_current(area);
      if (gtk_gl_area_get_error(area) != nullptr)
         throw std::runtime_error("Error. gtk_gl_area_get_error failed");

      view_state& state = *reinterpret_cast<view_state*>(user_data);
      glClearColor(state._bkd.red, state._bkd.green, state._bkd.blue, state._bkd.alpha);
      glClear(GL_COLOR_BUFFER_BIT);
      if (state._xface = GrGLMakeNativeInterface(); state._xface == nullptr)
         throw std::runtime_error("Error. GLMakeNativeInterface failed");
      if (state._ctx = GrDirectContext::MakeGL(state._xface); state._ctx == nullptr)
         throw std::runtime_error("Error. GrDirectContext::MakeGL failed");
   }

   gboolean render(GtkGLArea* area, GdkGLContext* context, gpointer user_data)
   {
      view_state& state = *reinterpret_cast<view_state*>(user_data);
      auto error = [](char const* msg) { throw std::runtime_error(msg); };

      auto draw_f =
         [&]()
         {
            if (!state._surface)
            {
               GrGLint buffer;
               glGetIntegerv(GL_FRAMEBUFFER_BINDING, &buffer);
               GrGLFramebufferInfo info;
               info.fFBOID = (GrGLuint) buffer;
               SkColorType colorType = kRGBA_8888_SkColorType;

               info.fFormat = GL_RGBA8;
               GrBackendRenderTarget target(
                  state._size.x*state._scale
                , state._size.y*state._scale
                , 0, 8, info
               );

               state._surface =
                  SkSurface::MakeFromBackendRenderTarget(
                     state._ctx.get(), target,
                     kBottomLeft_GrSurfaceOrigin, colorType, nullptr, nullptr
                  );

               if (!state._surface)
                  error("Error: SkSurface::MakeRenderTarget returned null");
            }

            SkCanvas* gpu_canvas = state._surface->getCanvas();
            gpu_canvas->save();
            gpu_canvas->scale(state._scale, state._scale);
            auto cnv = canvas{gpu_canvas};

            draw(cnv);

            gpu_canvas->restore();
            state._surface->flush();
         };

      auto start = std::chrono::steady_clock::now();
      draw_f();
      auto stop = std::chrono::steady_clock::now();
      elapsed_ = std::chrono::duration<double>{stop - start}.count();

      return true;
   }

   gboolean animate(gpointer user_data)
   {
      GtkWidget* da = GTK_WIDGET(user_data);
      gtk_widget_queue_draw(da);
      return true;
   }

   static auto proc = &glXGetProcAddress;

   void activate(GtkApplication* app, gpointer user_data)
   {
      auto error = [](char const* msg) { throw std::runtime_error(msg); };
      if (!proc)
         error("Error: glXGetProcAddress is null");

      view_state& state = *reinterpret_cast<view_state*>(user_data);
      auto* window = gtk_application_window_new(app);
      gtk_window_set_title(GTK_WINDOW(window), "Drawing Area");

      g_signal_connect(window, "destroy", G_CALLBACK(close_window), user_data);

      GtkWidget* widget = nullptr;
      try
      {
         // create a GtkGLArea instance
         GtkWidget* gl_area = gtk_gl_area_new();
         widget = gl_area;
         gtk_container_add(GTK_CONTAINER(window), gl_area);

         g_signal_connect(gl_area, "render", G_CALLBACK(render), user_data);
         g_signal_connect(gl_area, "realize", G_CALLBACK(realize), user_data);

         gtk_window_resize(GTK_WINDOW(window), state._size.x, state._size.y);
         gtk_widget_show_all(window);
      }
      catch (std::runtime_error const& e)
      {
         // GPU rendering not available
      }


      auto w = gtk_widget_get_window(GTK_WIDGET(window));
      state._scale = gdk_window_get_scale_factor(w);

      if (widget && state._animate)
         state._timer_id = g_timeout_add(1000 / 60, animate, widget);
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

   // This is declared in font.hpp
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
   state._size = window_size;
   state._animate = animate;
   state._bkd = background_color;

   auto* app = gtk_application_new("org.gtk-skia.example", G_APPLICATION_FLAGS_NONE);
   g_signal_connect(app, "activate", G_CALLBACK(activate), &state);
   int status = g_application_run(G_APPLICATION(app), argc, const_cast<char**>(argv));
   g_object_unref(app);

   return status;
}



