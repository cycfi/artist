/*=============================================================================
   Copyright (c) 2016-2023 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include "../../../app.hpp"
#include <gtk/gtk.h>
#include <cairo.h>
#include <chrono>
#include <cstdlib>
#include <cstdio>

using namespace cycfi::artist;
float elapsed_ = 0;  // rendering elapsed time

namespace
{
   struct view_state
   {
      extent            _size      = {};
      float             _scale     = 1.0f;
      bool              _animate   = false;
      color             _bkd       = colors::white;
      guint             _timer_id  = 0;

      GtkWidget*        _da        = nullptr;
      GtkWindow*        _window    = nullptr;

      // First-resize guard: GDK/XWayland emits a spurious half-size configure
      // on the very first user resize. We prime it away at startup.
      bool                  _primed     = false;
   };

   void close_window(GtkWidget*, gpointer user_data)
   {
      view_state& state = *reinterpret_cast<view_state*>(user_data);
      if (state._timer_id)
         g_source_remove(state._timer_id);
   }

   gboolean on_configure(GtkWidget* widget, GdkEventConfigure*, gpointer user_data)
   {
      view_state& state = *reinterpret_cast<view_state*>(user_data);

      float new_w = float(gtk_widget_get_allocated_width(widget));
      float new_h = float(gtk_widget_get_allocated_height(widget));

      // Suppress the spurious half-size event that GDK/XWayland emits on the
      // first resize. We prime it away at startup; this guard makes that
      // programmatic prime invisible (no visual glitch).
      if (!state._primed && state._size.x > 0)
      {
         if (new_w / state._size.x < 0.6f)
         {
            state._primed = true;
            return true;
         }
      }
      state._primed = true;

      state._size = {new_w, new_h};

      return true;
   }

   gboolean on_draw(GtkWidget* /*widget*/, cairo_t* cr, gpointer user_data)
   {
      view_state& state = *reinterpret_cast<view_state*>(user_data);

      auto start = std::chrono::steady_clock::now();

      // GDK already applies the device scale to cr, so draw() uses logical
      // coordinates and Cairo renders at full physical resolution on HiDPI.
      auto cnv = canvas{cr};
      draw(cnv);

      auto stop = std::chrono::steady_clock::now();
      elapsed_ = std::chrono::duration<double>{stop - start}.count();

      return false;
   }

   gboolean animate_cb(gpointer user_data)
   {
      gtk_widget_queue_draw(GTK_WIDGET(user_data));
      return true;
   }

   void activate(GtkApplication* app, gpointer user_data)
   {
      view_state& state = *reinterpret_cast<view_state*>(user_data);

      auto* window = gtk_application_window_new(app);
      state._window = GTK_WINDOW(window);
      gtk_window_set_title(GTK_WINDOW(window), "Artist (gtk3 cairo)");
      g_signal_connect(window, "destroy", G_CALLBACK(close_window), user_data);

      auto* da = gtk_drawing_area_new();
      state._da = da;
      gtk_container_add(GTK_CONTAINER(window), da);

      g_signal_connect(da, "configure-event", G_CALLBACK(on_configure), user_data);
      g_signal_connect(da, "draw",            G_CALLBACK(on_draw),      user_data);

      gtk_widget_set_events(da, gtk_widget_get_events(da) | GDK_BUTTON_PRESS_MASK);

      gtk_window_resize(GTK_WINDOW(window), state._size.x, state._size.y);
      gtk_widget_show_all(window);

      // Get HiDPI scale from the realized GDK window
      auto* gdk_win = gtk_widget_get_window(GTK_WIDGET(window));
      state._scale = float(gdk_window_get_scale_factor(gdk_win));

      if (state._animate)
         state._timer_id = g_timeout_add(1000/60, animate_cb, da);

      // Prime away the spurious half-size GDK/XWayland first-resize event by
      // triggering a programmatic resize before the user can grab the handle.
      // The bad event fires and is silently suppressed in on_configure.
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
   state._size      = window_size;
   state._animate   = animate;
   state._bkd       = background_color;

   auto* app = gtk_application_new("org.cycfi.artist.gtk3cairo",
                                   G_APPLICATION_DEFAULT_FLAGS);
   g_signal_connect(app, "activate", G_CALLBACK(activate), &state);
   int status = g_application_run(G_APPLICATION(app), argc, const_cast<char**>(argv));
   g_object_unref(app);
   return status;
}
