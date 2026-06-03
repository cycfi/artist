/*=============================================================================
   Copyright (c) 2016-2023 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include "../../app.hpp"
#include <gtk/gtk.h>
#include <cairo.h>
#include <chrono>

using namespace cycfi::artist;
float elapsed_ = 0;  // rendering elapsed time

namespace
{
   struct view_state
   {
      extent            _size = {};
      bool              _animate = false;
      color             _bkd = colors::white;
      guint             _timer_id = 0;
      cairo_surface_t*  _surface = nullptr;
   };

   void close_window(GtkWidget*, gpointer user_data)
   {
      view_state& state = *reinterpret_cast<view_state*>(user_data);
      if (state._timer_id)
         g_source_remove(state._timer_id);
      if (state._surface)
      {
         cairo_surface_destroy(state._surface);
         state._surface = nullptr;
      }
   }

   gboolean on_configure(GtkWidget* widget, GdkEventConfigure*, gpointer user_data)
   {
      view_state& state = *reinterpret_cast<view_state*>(user_data);
      if (state._surface)
         cairo_surface_destroy(state._surface);

      state._surface = gdk_window_create_similar_surface(
         gtk_widget_get_window(widget),
         CAIRO_CONTENT_COLOR,
         gtk_widget_get_allocated_width(widget),
         gtk_widget_get_allocated_height(widget)
      );
      return true;
   }

   gboolean on_draw(GtkWidget* widget, cairo_t* cr, gpointer user_data)
   {
      view_state& state = *reinterpret_cast<view_state*>(user_data);

      auto start = std::chrono::steady_clock::now();

      // Blit backing surface then draw into cr.
      cairo_set_source_surface(cr, state._surface, 0, 0);
      cairo_paint(cr);

      auto cnv = canvas{cr};
      draw(cnv);

      auto stop = std::chrono::steady_clock::now();
      elapsed_ = std::chrono::duration<double>{stop - start}.count();

      return false;
   }

   gboolean animate(gpointer user_data)
   {
      GtkWidget* da = GTK_WIDGET(user_data);
      gtk_widget_queue_draw(da);
      return true;
   }

   void activate(GtkApplication* app, gpointer user_data)
   {
      view_state& state = *reinterpret_cast<view_state*>(user_data);

      auto* window = gtk_application_window_new(app);
      gtk_window_set_title(GTK_WINDOW(window), "Artist");
      g_signal_connect(window, "destroy", G_CALLBACK(close_window), user_data);

      auto* da = gtk_drawing_area_new();
      gtk_container_add(GTK_CONTAINER(window), da);

      g_signal_connect(da, "configure-event", G_CALLBACK(on_configure), user_data);
      g_signal_connect(da, "draw",            G_CALLBACK(on_draw),      user_data);

      gtk_widget_set_events(da,
         gtk_widget_get_events(da) | GDK_BUTTON_PRESS_MASK);

      gtk_window_resize(GTK_WINDOW(window), state._size.x, state._size.y);
      gtk_widget_show_all(window);

      if (state._animate)
         state._timer_id = g_timeout_add(1000 / 60, animate, da);
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
   state._size    = window_size;
   state._animate = animate;
   state._bkd     = background_color;

   auto* app = gtk_application_new("org.gtk-cairo.example", G_APPLICATION_FLAGS_NONE);
   g_signal_connect(app, "activate", G_CALLBACK(activate), &state);
   int status = g_application_run(G_APPLICATION(app), argc, const_cast<char**>(argv));
   g_object_unref(app);
   return status;
}
