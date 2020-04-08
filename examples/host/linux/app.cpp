/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include "../../app.hpp"
#include <GL/glx.h> // $$$

# include "GrContext.h"
# include "gl/GrGLInterface.h"
# include "SkImage.h"
# include "SkSurface.h"

// # include "SkBitmap.h"
// # include "SkData.h"
// # include "SkImage.h"
// # include "SkPicture.h"
// # include "SkSurface.h"
// # include "SkCanvas.h"
// # include "SkPath.h"
// # include "GrBackendSurface.h"

using namespace cycfi::artist;
float elapsed_ = 0;  // rendering elapsed time

namespace
{
   extent _size = {};
   float  _scale = 1.0;
   bool   _animate = false;

   void close_window()
   {
   }

//   float get_scale(GtkWidget* widget)
//   {
//      //auto gdk_win = gtk_widget_get_window(widget);
//      return 1.0f / gdk_window_get_scale_factor(widget);
//   }

   gboolean render(GtkGLArea* area, GdkGLContext* context)
   {
      auto error = [](char const* msg) { throw std::runtime_error(msg); };

      // inside this function it's safe to use GL; the given
      // [gdk.GLContext.GLContext|gdk.GLContext] has been made current to the drawable
      // surface used by the [gtk.GLArea.GLArea|gtk.GLArea] and the viewport has
      // already been set to be the size of the allocation

      // // we can start by clearing the buffer
      // glClearColor(0, 0, 1, 1);
      // glClear(GL_COLOR_BUFFER_BIT);

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

      // we completed our drawing; the draw commands will be
      // flushed at the end of the signal emission chain, and
      // the buffers will be drawn on the window
      return true;
   }

   void activate(GtkApplication* app, gpointer user_data)
   {
      // $$$ Somehow, this prevents us from having linker errors $$$
      // $$$ Without this, we get undefined reference to `glXGetCurrentContext' $$$
      static auto proc = &glXGetProcAddress;

      extent const& p = *reinterpret_cast<extent const*>(user_data);
      auto* window = gtk_application_window_new(app);
      gtk_window_set_title(GTK_WINDOW(window), "Drawing Area");

      g_signal_connect(window, "destroy", G_CALLBACK(close_window), nullptr);

      // create a GtkGLArea instance
      auto* gl_area = gtk_gl_area_new();
      gtk_container_add(GTK_CONTAINER(window), gl_area);

      // connect to the "render" signal
      //if (_animate)
         g_signal_connect(gl_area, "render", G_CALLBACK(render), nullptr);

      //auto win = GTK_WINDOW(window);
      gtk_window_resize(GTK_WINDOW(window), p.x, p.y);


      gtk_widget_show_all(window);

      auto w = gtk_widget_get_window(GTK_WIDGET(window));
      auto scale = gdk_window_get_scale_factor(w); //get_scale(window);
      _scale = scale;
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

   auto* app = gtk_application_new("org.gtk-skia.example", G_APPLICATION_FLAGS_NONE);
   g_signal_connect(app, "activate", G_CALLBACK(activate), &window_size);
   int status = g_application_run(G_APPLICATION(app), argc, const_cast<char**>(argv));
   g_object_unref(app);

   return status;
}

void stop_app()
{
}

void print_elapsed(canvas& cnv, point br)
{
   static font open_sans = font_descr{ "Open Sans", 12 };
   static int i = 0;
   static float t_elapsed = 0;
   static float c_elapsed = 0;

   if (++i == 30)
   {
      i = 0;
      c_elapsed = t_elapsed / 30;
      t_elapsed = 0;
   }
   else
   {
      t_elapsed += elapsed_;
   }

   if (c_elapsed)
   {
      cnv.fill_style(rgba(220, 220, 220, 200));
      cnv.font(open_sans);
      cnv.text_align(cnv.right | cnv.bottom);
      cnv.fill_text(std::to_string(1 / c_elapsed) + " fps", { br.x, br.y });
   }
}


