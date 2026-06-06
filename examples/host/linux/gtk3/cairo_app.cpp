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
      extent            _size      = {};
      float             _scale     = 1.0f;
      bool              _animate   = false;
      bool              _resize_log = false;
      color             _bkd       = colors::white;
      guint             _timer_id  = 0;

      GtkWidget*        _da        = nullptr;
      GtkWindow*        _window    = nullptr;

      // First-resize guard: GDK/XWayland emits a spurious half-size configure
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
            if (state._resize_log)
               std::fprintf(stderr, "[resize] suppressed spurious half-size: logical=%.0fx%.0f\n",
                  new_w, new_h);
            return true;
         }
      }
      state._primed = true;

      state._size = {new_w, new_h};

      if (state._resize_log)
         std::fprintf(stderr, "[resize] logical=%.0fx%.0f  scale=%.1f\n",
            state._size.x, state._size.y, state._scale);

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

      // Bench: record end-to-end resize+draw time after a pending resize
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
               "[bench] gtk3 cairo  N=%zu"
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
            gtk_widget_queue_draw(state._da);
         }
      }

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
      state._gapp = G_APPLICATION(app);

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
            gtk_widget_queue_draw(st._da);
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
   state._size      = window_size;
   state._animate   = animate;
   state._bkd       = background_color;
   state._resize_log = std::getenv("ARTIST_RESIZE_LOG") != nullptr;

   auto* app = gtk_application_new("org.cycfi.artist.gtk3cairo",
                                   G_APPLICATION_DEFAULT_FLAGS);
   g_signal_connect(app, "activate", G_CALLBACK(activate), &state);
   int status = g_application_run(G_APPLICATION(app), argc, const_cast<char**>(argv));
   g_object_unref(app);
   return status;
}
