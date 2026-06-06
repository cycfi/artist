/*=============================================================================
   Copyright (c) 2016-2023 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include "../../../app.hpp"
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xresource.h>
#include <cairo.h>
#include <cairo-xlib.h>
#include <chrono>
#include <thread>
#include <cmath>
#include <cstdlib>
#include <stdexcept>

using namespace cycfi::artist;
float elapsed_ = 0;  // rendering elapsed time

namespace
{
   struct app_state
   {
      Display*         display = nullptr;
      int              screen  = 0;
      Window           window  = 0;
      Atom             wm_delete = 0;
      cairo_surface_t* surface = nullptr;

      extent           size    = {};
      double           scale   = 1.0;
      color            bkd     = colors::white;
      bool             animate = false;
      bool             running = true;
   };

   // -------------------------------------------------------------------------
   // HiDPI scale from the X resource manager (Xft.dpi / 96). GNOME sets Xft.dpi
   // on XWayland to 96 × the display scale (e.g. 192 for 2×). Defaults to 1.0.
   double get_scale(Display* display)
   {
      double scale = 1.0;
      if (char* rms = XResourceManagerString(display))
      {
         if (XrmDatabase db = XrmGetStringDatabase(rms))
         {
            XrmValue value;
            char* type = nullptr;
            if (XrmGetResource(db, "Xft.dpi", "Xft.Dpi", &type, &value) && value.addr)
            {
               if (double dpi = std::atof(value.addr); dpi > 0)
                  scale = dpi / 96.0;
            }
            XrmDestroyDatabase(db);
         }
      }
      return scale;
   }

   // -------------------------------------------------------------------------
   // Render into the X11 window via Cairo's Xlib backend
   void render(app_state& state)
   {
      auto start = std::chrono::steady_clock::now();

      auto* cr  = cairo_create(state.surface);
      auto  cnv = canvas{cr};
      draw(cnv);
      cairo_destroy(cr);

      auto stop = std::chrono::steady_clock::now();
      elapsed_ = std::chrono::duration<double>{stop - start}.count();

      cairo_surface_flush(state.surface);
      XFlush(state.display);
   }

   // -------------------------------------------------------------------------
   // Dispatch a single X11 event
   void dispatch(app_state& state, XEvent& ev)
   {
      switch (ev.type)
      {
         case Expose:
            // Only redraw on the last contiguous expose event
            if (ev.xexpose.count == 0)
               render(state);
            break;

         case ConfigureNotify:
            cairo_xlib_surface_set_size(
               state.surface, ev.xconfigure.width, ev.xconfigure.height);
            break;

         case ClientMessage:
            if (Atom(ev.xclient.data.l[0]) == state.wm_delete)
               state.running = false;
            break;

         default:
            break;
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
   int         /*argc*/
 , char const* /*argv*/[]
 , extent      window_size
 , color       background_color
 , bool        animate
)
{
   app_state state;
   state.size    = window_size;
   state.bkd     = background_color;
   state.animate = animate;

   state.display = XOpenDisplay(nullptr);
   if (!state.display)
      throw std::runtime_error("Failed to open X display");

   state.screen = DefaultScreen(state.display);
   Window root  = RootWindow(state.display, state.screen);

   state.scale = get_scale(state.display);

   // Physical pixel size = logical size × scale. The window is created at
   // physical size; Cairo's device scale lets draw() use logical coordinates.
   int const w = int(std::lround(state.size.x * state.scale));
   int const h = int(std::lround(state.size.y * state.scale));

   state.window = XCreateSimpleWindow(
      state.display, root,
      0, 0, w, h, 0,
      BlackPixel(state.display, state.screen),
      WhitePixel(state.display, state.screen)
   );

   // Window title
   XStoreName(state.display, state.window, "Artist");

   // Fixed-size window: pin min == max so the WM disallows resize/maximize
   if (XSizeHints* hints = XAllocSizeHints())
   {
      hints->flags      = PMinSize | PMaxSize;
      hints->min_width  = hints->max_width  = w;
      hints->min_height = hints->max_height = h;
      XSetWMNormalHints(state.display, state.window, hints);
      XFree(hints);
   }

   // Listen for paint, resize, and the window-close protocol
   XSelectInput(state.display, state.window, ExposureMask | StructureNotifyMask);
   state.wm_delete = XInternAtom(state.display, "WM_DELETE_WINDOW", False);
   XSetWMProtocols(state.display, state.window, &state.wm_delete, 1);

   XMapWindow(state.display, state.window);

   // Cairo Xlib backing surface, at physical size with device scale applied so
   // draw() works in logical coordinates (matches the Wayland/macOS hosts).
   state.surface = cairo_xlib_surface_create(
      state.display, state.window,
      DefaultVisual(state.display, state.screen),
      w, h
   );
   cairo_surface_set_device_scale(state.surface, state.scale, state.scale);

   // Event loop
   using clock = std::chrono::steady_clock;
   auto next_frame = clock::now();
   constexpr auto frame_interval = std::chrono::milliseconds(1000 / 60);

   while (state.running)
   {
      if (state.animate)
      {
         // Drain pending events without blocking, then render a throttled frame
         while (XPending(state.display))
         {
            XEvent ev;
            XNextEvent(state.display, &ev);
            dispatch(state, ev);
         }
         render(state);
         next_frame += frame_interval;
         std::this_thread::sleep_until(next_frame);
      }
      else
      {
         // Block until the next event when idle
         XEvent ev;
         XNextEvent(state.display, &ev);
         dispatch(state, ev);
      }
   }

   cairo_surface_destroy(state.surface);
   XDestroyWindow(state.display, state.window);
   XCloseDisplay(state.display);
   return 0;
}
