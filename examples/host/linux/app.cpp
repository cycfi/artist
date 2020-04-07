/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include "../../app.hpp"

using namespace cycfi::artist;
float elapsed_ = 0;  // rendering elapsed time

namespace
{
   void close_window(void)
   {
   }

   gboolean render(GtkGLArea* area, GdkGLContext* context)
   {
      // inside this function it's safe to use GL; the given
      // [gdk.GLContext.GLContext|gdk.GLContext] has been made current to the drawable
      // surface used by the [gtk.GLArea.GLArea|gtk.GLArea] and the viewport has
      // already been set to be the size of the allocation

      // we can start by clearing the buffer
      glClearColor(0, 0, 1, 1);
      glClear(GL_COLOR_BUFFER_BIT);

      // draw your object

      // we completed our drawing; the draw commands will be
      // flushed at the end of the signal emission chain, and
      // the buffers will be drawn on the window
      return true;
   }

   void activate(GtkApplication* app, gpointer user_data)
   {
      auto* window = gtk_application_window_new(app);
      gtk_window_set_title(GTK_WINDOW (window), "Drawing Area");

      g_signal_connect(window, "destroy", G_CALLBACK(close_window), nullptr);

      // create a GtkGLArea instance
      auto* gl_area = gtk_gl_area_new();
      gtk_container_add(GTK_CONTAINER(window), gl_area);

      // connect to the "render" signal
      g_signal_connect(gl_area, "render", G_CALLBACK(render), nullptr);

      gtk_widget_show_all(window);
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
   auto* app = gtk_application_new("org.gtk.example", G_APPLICATION_FLAGS_NONE);
   g_signal_connect(app, "activate", G_CALLBACK(activate), nullptr);
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


