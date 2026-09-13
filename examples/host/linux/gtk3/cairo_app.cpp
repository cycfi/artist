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
      extent            _size       = {};
      extent            _prime_size = {};
      float             _scale      = 1.0f;
      bool              _animate    = false;
      color             _bkd        = colors::white;
      guint             _timer_id   = 0;

      GtkWidget*        _da         = nullptr;
      GtkWindow*        _window     = nullptr;

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
      auto start = std::chrono::steady_clock::now();

      // GDK already applies the device scale to cr, so draw() uses logical
      // coordinates and Cairo renders at full physical resolution on HiDPI.
      auto cnv = canvas{cr};
      draw(cnv);

      auto stop = std::chrono::steady_clock::now();
      elapsed_ = std::chrono::duration<double>{stop - start}.count();

      return false;
   }

   // ARTIST_PERF frames: draw and present outside GTK's paint cycle, which
   // would otherwise present each frame after the draw signal returns, paced
   // by the frame clock. Measuring runs GDK's X11 backend (see run_app), where
   // the frame goes straight to the X server; the frame time runs until the
   // server has finished it.
   gboolean perf_frame(gpointer user_data)
   {
      view_state& state = *reinterpret_cast<view_state*>(user_data);
      auto* gdk_win = gtk_widget_get_window(state._da);
      int const w = gtk_widget_get_allocated_width(state._da);
      int const h = gtk_widget_get_allocated_height(state._da);
      if (!gdk_win || w <= 0 || h <= 0)
         return G_SOURCE_CONTINUE;

      cairo_rectangle_int_t const area = {0, 0, w, h};
      auto* region = cairo_region_create_rectangle(&area);

      auto start = std::chrono::steady_clock::now();
      auto* frame = gdk_window_begin_draw_frame(gdk_win, region);
      auto* cr = gdk_drawing_context_get_cairo_context(frame);
      {
         auto cnv = canvas{cr};
         draw(cnv);
      }
      cairo_surface_flush(cairo_get_target(cr));
      gdk_window_end_draw_frame(gdk_win, frame);
      gdk_display_sync(gdk_window_get_display(gdk_win));
      auto stop = std::chrono::steady_clock::now();
      cairo_region_destroy(region);

      elapsed_ = std::chrono::duration<double>{stop - start}.count();
      perf_record(elapsed_, int(w * state._scale + 0.5f), int(h * state._scale + 0.5f));
      return G_SOURCE_CONTINUE;
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

      if (state._animate && !perf_enabled())
         state._timer_id = g_timeout_add(1000/60, animate_cb, da);

      // Prime away the spurious half-size GDK/XWayland first-resize event by
      // triggering a programmatic resize before the user can grab the handle.
      // The bad event fires and is silently suppressed in on_configure. The
      // restore uses the size from before the prime, since configure events
      // may already have moved _size to the primed width. When measuring
      // (ARTIST_PERF), frames start only once the window is back at its size.
      g_timeout_add(150, [](gpointer data) -> gboolean {
         view_state& st = *reinterpret_cast<view_state*>(data);
         st._prime_size = st._size;
         gtk_window_resize(st._window, int(st._size.x) + 2, int(st._size.y));
         g_timeout_add(50, [](gpointer data2) -> gboolean {
            view_state& st2 = *reinterpret_cast<view_state*>(data2);
            gtk_window_resize(st2._window,
               int(st2._prime_size.x), int(st2._prime_size.y));
            if (perf_enabled())
               g_timeout_add(100, [](gpointer data3) -> gboolean {
                  view_state& st3 = *reinterpret_cast<view_state*>(data3);
                  st3._timer_id = g_idle_add(perf_frame, data3);
                  return FALSE;
               }, data2);
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

   // GDK's Wayland backend commits a frame on its own frame clock, so a frame
   // drawn outside the paint cycle is not presented right away. Measuring
   // needs a present it can wait on, which GDK's X11 backend provides.
   if (perf_enabled())
      gdk_set_allowed_backends("x11");

   auto* app = gtk_application_new("org.cycfi.artist.gtk3cairo",
                                   G_APPLICATION_DEFAULT_FLAGS);
   g_signal_connect(app, "activate", G_CALLBACK(activate), &state);
   int status = g_application_run(G_APPLICATION(app), argc, const_cast<char**>(argv));
   g_object_unref(app);
   return status;
}
