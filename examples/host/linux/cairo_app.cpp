/*=============================================================================
   Copyright (c) 2016-2023 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include "../../app.hpp"
#include <wayland-client.h>
#include <libdecor.h>
#include <cairo.h>
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
      wl_display*    display    = nullptr;
      wl_registry*   registry   = nullptr;
      wl_compositor* compositor = nullptr;
      wl_shm*        shm        = nullptr;
      wl_output*     output     = nullptr;

      // libdecor handles xdg-shell and decorations
      libdecor*       decor   = nullptr;
      libdecor_frame* frame   = nullptr;
      wl_surface*     surface = nullptr;

      // Shared-memory buffer
      wl_shm_pool*   pool         = nullptr;
      wl_buffer*     buffer       = nullptr;
      void*          shm_data     = nullptr;
      int            shm_fd       = -1;
      size_t         shm_size     = 0;

      // App parameters
      extent         size         = {};
      int            scale        = 1;
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
   // Forward declaration for frame callback
   void frame_done(void* data, wl_callback* cb, uint32_t time);

   constexpr wl_callback_listener frame_listener = {
      .done = frame_done
   };

   // -------------------------------------------------------------------------
   // Render into shm buffer via Cairo, then commit to compositor
   void render(app_state& state)
   {
      if (!state.configured || !state.buf_released)
         return;

      int const w = int(state.size.x) * state.scale;
      int const h = int(state.size.y) * state.scale;

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

      state.buf_released = false;
      wl_surface_set_buffer_scale(state.surface, state.scale);
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
   // wl_output listener — reads integer HiDPI scale factor
   void output_geometry(void*, wl_output*, int32_t, int32_t, int32_t, int32_t,
                        int32_t, const char*, const char*, int32_t) {}
   void output_mode(void*, wl_output*, uint32_t, int32_t, int32_t, int32_t) {}
   void output_done(void*, wl_output*) {}
   void output_scale(void* data, wl_output*, int32_t factor)
   {
      static_cast<app_state*>(data)->scale = factor;
   }

   constexpr wl_output_listener output_listener = {
      .geometry = output_geometry,
      .mode     = output_mode,
      .done     = output_done,
      .scale    = output_scale
   };

   // -------------------------------------------------------------------------
   // wl_registry listener — binds compositor, shm, output
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
      else if (strcmp(interface, wl_output_interface.name) == 0)
      {
         state.output = static_cast<wl_output*>(
            wl_registry_bind(registry, name, &wl_output_interface, 2));
         wl_output_add_listener(state.output, &output_listener, &state);
      }
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
      int const    w      = int(state.size.x) * state.scale;
      int const    h      = int(state.size.y) * state.scale;
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
   state.frame   = libdecor_decorate(state.decor, state.surface, &frame_iface, &state);

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
   if (state.buffer)    wl_buffer_destroy(state.buffer);
   if (state.pool)      wl_shm_pool_destroy(state.pool);
   if (state.shm_data)  munmap(state.shm_data, state.shm_size);
   if (state.shm_fd >= 0) close(state.shm_fd);
   libdecor_frame_unref(state.frame);
   libdecor_unref(state.decor);
   if (state.surface)   wl_surface_destroy(state.surface);
   if (state.output)    wl_output_destroy(state.output);
   if (state.shm)       wl_shm_destroy(state.shm);
   if (state.compositor) wl_compositor_destroy(state.compositor);
   wl_registry_destroy(state.registry);
   wl_display_disconnect(state.display);

   return 0;
}
