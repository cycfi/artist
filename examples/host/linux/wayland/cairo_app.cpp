/*=============================================================================
   Copyright (c) 2016-2023 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include "../../app.hpp"
#include <wayland-client.h>
#include <libdecor.h>
#include "fractional-scale-v1-client-protocol.h"
#include "viewporter-client-protocol.h"
#include <cairo.h>
#include <cmath>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <chrono>
#include <cstring>
#include <stdexcept>

using namespace cycfi::artist;
float elapsed_ = 0;  // rendering elapsed time

namespace
{
   struct app_state
   {
      // Wayland globals
      wl_display*                    display       = nullptr;
      wl_registry*                   registry      = nullptr;
      wl_compositor*                 compositor    = nullptr;
      wl_shm*                        shm           = nullptr;
      wp_viewporter*                 viewporter    = nullptr;
      wp_fractional_scale_manager_v1* frac_manager = nullptr;

      // libdecor handles xdg-shell and decorations
      libdecor*                      decor         = nullptr;
      libdecor_frame*                frame         = nullptr;
      wl_surface*                    surface       = nullptr;
      wp_viewport*                   viewport      = nullptr;
      wp_fractional_scale_v1*        frac_scale    = nullptr;

      // Shared-memory buffer
      wl_shm_pool*   pool         = nullptr;
      wl_buffer*     buffer       = nullptr;
      void*          shm_data     = nullptr;
      int            shm_fd       = -1;
      size_t         shm_size     = 0;

      // App parameters
      extent         size         = {};
      float          scale        = 1.0f;  // fractional scale (1/120ths → float)
      color          bkd          = colors::white;
      bool           animate      = false;

      // State flags
      bool           running      = true;
      bool           configured   = false;
      bool           buf_released = true;

      // Frame timing
      uint32_t       last_frame_ms  = 0;       // compositor timestamp of last render
      uint32_t       frame_interval = 1000/60; // target: 60 fps
   };

   // -------------------------------------------------------------------------
   // Forward declarations
   void frame_done(void* data, wl_callback* cb, uint32_t time);
   void create_buffer(app_state& state);
   void render(app_state& state);

   constexpr wl_callback_listener frame_listener = {
      .done = frame_done
   };

   // -------------------------------------------------------------------------
   // Render into shm buffer via Cairo, then commit to compositor
   void render(app_state& state)
   {
      if (!state.configured || !state.buf_released)
         return;

      int const w = int(std::ceil(state.size.x * state.scale));
      int const h = int(std::ceil(state.size.y * state.scale));

      auto start = std::chrono::steady_clock::now();

      auto* surf = cairo_image_surface_create_for_data(
         static_cast<unsigned char*>(state.shm_data),
         CAIRO_FORMAT_ARGB32,
         w, h, w * 4
      );
      // Device scale lets draw() use logical coordinates while rendering
      // at full physical resolution on HiDPI displays.
      cairo_surface_set_device_scale(surf, state.scale, state.scale);

      auto* cr  = cairo_create(surf);
      auto  cnv = canvas{cr};
      draw(cnv);
      cairo_destroy(cr);
      cairo_surface_destroy(surf);

      auto stop = std::chrono::steady_clock::now();
      elapsed_ = std::chrono::duration<double>{stop - start}.count();

      // viewport maps the physical buffer back to logical size, handling
      // both integer and fractional scales without wl_surface_set_buffer_scale.
      if (state.viewport)
         wp_viewport_set_destination(state.viewport,
            int(state.size.x), int(state.size.y));

      state.buf_released = false;
      wl_surface_attach(state.surface, state.buffer, 0, 0);
      wl_surface_damage_buffer(state.surface, 0, 0, w, h);
      wl_surface_commit(state.surface);
      wl_display_flush(state.display);
   }

   // -------------------------------------------------------------------------
   // wl_callback listener — drives the animation loop
   void frame_done(void* data, wl_callback* cb, uint32_t time)
   {
      wl_callback_destroy(cb);
      auto& state = *static_cast<app_state*>(data);

      // Request next callback before any commit so it's always armed
      auto* next = wl_surface_frame(state.surface);
      wl_callback_add_listener(next, &frame_listener, &state);

      // Throttle: if the compositor fires faster than our target, skip the
      // expensive draw but still commit to arm the next frame callback.
      uint32_t const delta = time - state.last_frame_ms;
      if (state.last_frame_ms != 0 && delta < state.frame_interval)
      {
         wl_surface_commit(state.surface);
         return;
      }

      state.last_frame_ms = time;
      render(state);  // render does attach + damage + commit; measures elapsed_
   }

   // -------------------------------------------------------------------------
   // wl_buffer listener
   void buffer_release(void* data, wl_buffer* /*buf*/)
   {
      static_cast<app_state*>(data)->buf_released = true;
   }

   constexpr wl_buffer_listener buffer_listener = {
      .release = buffer_release
   };

   // -------------------------------------------------------------------------
   // wp_fractional_scale_v1 listener — preferred_scale is in 1/120ths units
   // (120 = 1.0×, 180 = 1.5×, 240 = 2.0×, etc.)
   void frac_scale_preferred(void* data, wp_fractional_scale_v1* /*frac*/,
                             uint32_t scale_120)
   {
      auto& state = *static_cast<app_state*>(data);
      state.scale = float(scale_120) / 120.0f;
      // Recreate buffer at new physical size and redraw
      if (state.configured)
      {
         create_buffer(state);
         render(state);
      }
   }

   constexpr wp_fractional_scale_v1_listener frac_scale_listener = {
      .preferred_scale = frac_scale_preferred
   };

   // -------------------------------------------------------------------------
   // wl_registry listener — binds compositor, shm, viewporter, fractional scale
   void registry_global(void* data, wl_registry* registry,
                        uint32_t name, const char* interface, uint32_t /*version*/)
   {
      auto& state = *static_cast<app_state*>(data);

      if (strcmp(interface, wl_compositor_interface.name) == 0)
         state.compositor = static_cast<wl_compositor*>(
            wl_registry_bind(registry, name, &wl_compositor_interface, 4));
      else if (strcmp(interface, wl_shm_interface.name) == 0)
         state.shm = static_cast<wl_shm*>(
            wl_registry_bind(registry, name, &wl_shm_interface, 1));
      else if (strcmp(interface, wp_viewporter_interface.name) == 0)
         state.viewporter = static_cast<wp_viewporter*>(
            wl_registry_bind(registry, name, &wp_viewporter_interface, 1));
      else if (strcmp(interface, wp_fractional_scale_manager_v1_interface.name) == 0)
         state.frac_manager = static_cast<wp_fractional_scale_manager_v1*>(
            wl_registry_bind(registry, name, &wp_fractional_scale_manager_v1_interface, 1));
   }

   void registry_global_remove(void*, wl_registry*, uint32_t) {}

   constexpr wl_registry_listener registry_listener = {
      .global        = registry_global,
      .global_remove = registry_global_remove
   };

   // -------------------------------------------------------------------------
   // Allocate a shared-memory buffer for the current logical size × scale
   void create_buffer(app_state& state)
   {
      // Physical pixel size: round up to avoid sub-pixel gaps
      int const    w      = int(std::ceil(state.size.x * state.scale));
      int const    h      = int(std::ceil(state.size.y * state.scale));
      int const    stride = w * 4;
      size_t const size   = size_t(stride) * h;

      if (state.buffer)    { wl_buffer_destroy(state.buffer);        state.buffer   = nullptr; }
      if (state.pool)      { wl_shm_pool_destroy(state.pool);        state.pool     = nullptr; }
      if (state.shm_data)  { munmap(state.shm_data, state.shm_size); state.shm_data = nullptr; }
      if (state.shm_fd >= 0) { close(state.shm_fd); state.shm_fd = -1; }

      state.shm_fd = memfd_create("artist-wayland", MFD_CLOEXEC);
      if (state.shm_fd < 0)
         throw std::runtime_error("memfd_create failed");
      if (ftruncate(state.shm_fd, off_t(size)) < 0)
         throw std::runtime_error("ftruncate failed");

      state.shm_size = size;
      state.shm_data = mmap(nullptr, size, PROT_READ | PROT_WRITE,
                            MAP_SHARED, state.shm_fd, 0);
      if (state.shm_data == MAP_FAILED)
         throw std::runtime_error("mmap failed");

      state.pool   = wl_shm_create_pool(state.shm, state.shm_fd, int32_t(size));
      state.buffer = wl_shm_pool_create_buffer(
         state.pool, 0, w, h, stride, WL_SHM_FORMAT_ARGB8888);
      wl_buffer_add_listener(state.buffer, &buffer_listener, &state);
      state.buf_released = true;
   }

   // -------------------------------------------------------------------------
   // libdecor frame callbacks
   void libdecor_frame_configure(
      libdecor_frame*         frame,
      libdecor_configuration* config,
      void*                   data)
   {
      auto& state = *static_cast<app_state*>(data);

      // Use compositor-suggested size if provided, otherwise keep requested size
      int w = int(state.size.x);
      int h = int(state.size.y);
      libdecor_configuration_get_content_size(config, frame, &w, &h);

      state.size = {float(w), float(h)};

      auto* st = libdecor_state_new(w, h);
      libdecor_frame_commit(frame, st, config);
      libdecor_state_free(st);

      create_buffer(state);
      state.configured = true;
      render(state);
   }

   void libdecor_frame_close(libdecor_frame* /*frame*/, void* data)
   {
      static_cast<app_state*>(data)->running = false;
   }

   void libdecor_frame_commit(libdecor_frame* /*frame*/, void* /*data*/) {}

   libdecor_frame_interface frame_iface = {
      .configure     = libdecor_frame_configure,
      .close         = libdecor_frame_close,
      .commit        = libdecor_frame_commit,
   };

   // -------------------------------------------------------------------------
   // libdecor context error handler
   void libdecor_error(libdecor* /*context*/, libdecor_error error, const char* msg)
   {
      throw std::runtime_error(std::string("libdecor error: ") + msg);
   }

   libdecor_interface decor_iface = {
      .error = libdecor_error,
   };
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

   state.display = wl_display_connect(nullptr);
   if (!state.display)
      throw std::runtime_error("Failed to connect to Wayland display");

   state.registry = wl_display_get_registry(state.display);
   wl_registry_add_listener(state.registry, &registry_listener, &state);
   wl_display_roundtrip(state.display);  // receive globals
   wl_display_roundtrip(state.display);  // receive output scale

   if (!state.compositor || !state.shm)
      throw std::runtime_error("Missing required Wayland globals");

   // libdecor manages xdg-shell and window decorations
   state.decor   = libdecor_new(state.display, &decor_iface);
   state.surface = wl_compositor_create_surface(state.compositor);

   // Attach fractional scale + viewport for proper HiDPI support.
   // Falls back gracefully to scale=1.0 if the compositor lacks the protocol.
   if (state.frac_manager && state.viewporter)
   {
      state.frac_scale = wp_fractional_scale_manager_v1_get_fractional_scale(
         state.frac_manager, state.surface);
      wp_fractional_scale_v1_add_listener(
         state.frac_scale, &frac_scale_listener, &state);
      state.viewport = wp_viewporter_get_viewport(state.viewporter, state.surface);
   }

   state.frame = libdecor_decorate(state.decor, state.surface, &frame_iface, &state);

   libdecor_frame_set_title(state.frame, "Artist");
   libdecor_frame_set_app_id(state.frame, "org.cycfi.artist");

   // Fixed-size window: remove resize and maximize
   libdecor_frame_unset_capabilities(state.frame, LIBDECOR_ACTION_RESIZE);
   int const w = int(state.size.x);
   int const h = int(state.size.y);
   libdecor_frame_set_min_content_size(state.frame, w, h);
   libdecor_frame_set_max_content_size(state.frame, w, h);

   libdecor_frame_map(state.frame);

   // Pump until we get the configure event (sets configured = true)
   while (!state.configured)
   {
      if (libdecor_dispatch(state.decor, -1) < 0)
         throw std::runtime_error("libdecor_dispatch failed during init");
   }

   if (animate)
   {
      auto* cb = wl_surface_frame(state.surface);
      wl_callback_add_listener(cb, &frame_listener, &state);
      render(state);
   }

   // Main event loop
   while (state.running)
   {
      if (libdecor_dispatch(state.decor, -1) < 0)
         break;
   }

   // Tear down
   if (state.buffer)       wl_buffer_destroy(state.buffer);
   if (state.pool)         wl_shm_pool_destroy(state.pool);
   if (state.shm_data)     munmap(state.shm_data, state.shm_size);
   if (state.shm_fd >= 0)  close(state.shm_fd);
   if (state.frac_scale)   wp_fractional_scale_v1_destroy(state.frac_scale);
   if (state.viewport)     wp_viewport_destroy(state.viewport);
   if (state.frac_manager) wp_fractional_scale_manager_v1_destroy(state.frac_manager);
   if (state.viewporter)   wp_viewporter_destroy(state.viewporter);
   libdecor_frame_unref(state.frame);
   libdecor_unref(state.decor);
   if (state.surface)      wl_surface_destroy(state.surface);
   if (state.shm)          wl_shm_destroy(state.shm);
   if (state.compositor)   wl_compositor_destroy(state.compositor);
   wl_registry_destroy(state.registry);
   wl_display_disconnect(state.display);

   return 0;
}
