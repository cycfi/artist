/*=============================================================================
   Copyright (c) 2016-2023 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include "../../app.hpp"
#include <wayland-client.h>
#include "xdg-shell-client-protocol.h"
#include "xdg-decoration-unstable-v1-client-protocol.h"
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
      wl_display*                    display      = nullptr;
      wl_registry*                   registry     = nullptr;
      wl_compositor*                 compositor   = nullptr;
      wl_shm*                        shm          = nullptr;
      xdg_wm_base*                   wm_base      = nullptr;
      wl_output*                     output       = nullptr;
      zxdg_decoration_manager_v1*    deco_manager = nullptr;

      // Surface objects
      wl_surface*      surface      = nullptr;
      xdg_surface*     xdg_surf     = nullptr;
      xdg_toplevel*    toplevel     = nullptr;

      // Shared-memory buffer
      wl_shm_pool*     pool         = nullptr;
      wl_buffer*       buffer       = nullptr;
      void*            shm_data     = nullptr;
      int              shm_fd       = -1;
      size_t           shm_size     = 0;

      // App parameters
      extent           size         = {};
      int              scale        = 1;
      color            bkd          = colors::white;
      bool             animate      = false;

      // State flags
      bool             running      = true;
      bool             configured   = false;
      bool             buf_released = true;
   };

   // -------------------------------------------------------------------------
   // Forward declaration for frame callback
   void frame_done(void* data, wl_callback* cb, uint32_t time);

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

      auto* cr = cairo_create(surf);
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
   constexpr wl_callback_listener frame_listener = {
      .done = frame_done
   };

   void frame_done(void* data, wl_callback* cb, uint32_t /*time*/)
   {
      wl_callback_destroy(cb);
      auto& state = *static_cast<app_state*>(data);
      // Request the next frame before rendering to keep cadence tight
      auto* next = wl_surface_frame(state.surface);
      wl_callback_add_listener(next, &frame_listener, &state);
      render(state);
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
   // xdg_wm_base listener — must respond to ping or compositor kills the surface
   void wm_base_ping(void* /*data*/, xdg_wm_base* wm_base, uint32_t serial)
   {
      xdg_wm_base_pong(wm_base, serial);
   }

   constexpr xdg_wm_base_listener wm_base_listener = {
      .ping = wm_base_ping
   };

   // -------------------------------------------------------------------------
   // xdg_surface listener — ack configure, then do initial render
   void xdg_surface_configure(void* data, xdg_surface* xdg_surf, uint32_t serial)
   {
      auto& state = *static_cast<app_state*>(data);
      xdg_surface_ack_configure(xdg_surf, serial);
      state.configured = true;
      render(state);
   }

   constexpr xdg_surface_listener xdg_surface_lst = {
      .configure = xdg_surface_configure
   };

   // -------------------------------------------------------------------------
   // xdg_toplevel listener
   void toplevel_configure(void* /*data*/, xdg_toplevel*,
                           int32_t /*w*/, int32_t /*h*/, wl_array*)
   {
      // We ignore compositor-suggested sizes and keep our requested size.
   }

   void toplevel_close(void* data, xdg_toplevel*)
   {
      static_cast<app_state*>(data)->running = false;
   }

   constexpr xdg_toplevel_listener toplevel_listener = {
      .configure = toplevel_configure,
      .close     = toplevel_close
   };

   // -------------------------------------------------------------------------
   // wl_registry listener — binds global interfaces
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
      else if (strcmp(interface, xdg_wm_base_interface.name) == 0)
      {
         state.wm_base = static_cast<xdg_wm_base*>(
            wl_registry_bind(registry, name, &xdg_wm_base_interface, 1));
         xdg_wm_base_add_listener(state.wm_base, &wm_base_listener, nullptr);
      }
      else if (strcmp(interface, wl_output_interface.name) == 0)
      {
         state.output = static_cast<wl_output*>(
            wl_registry_bind(registry, name, &wl_output_interface, 2));
         wl_output_add_listener(state.output, &output_listener, &state);
      }
      else if (strcmp(interface, zxdg_decoration_manager_v1_interface.name) == 0)
      {
         state.deco_manager = static_cast<zxdg_decoration_manager_v1*>(
            wl_registry_bind(registry, name, &zxdg_decoration_manager_v1_interface, 1));
      }
   }

   void registry_global_remove(void*, wl_registry*, uint32_t) {}

   constexpr wl_registry_listener registry_listener = {
      .global        = registry_global,
      .global_remove = registry_global_remove
   };

   // -------------------------------------------------------------------------
   // Allocate a shared-memory buffer for the given logical size × scale
   void create_buffer(app_state& state)
   {
      int const    w      = int(state.size.x) * state.scale;
      int const    h      = int(state.size.y) * state.scale;
      int const    stride = w * 4;
      size_t const size   = size_t(stride) * h;

      // Tear down previous buffer if it exists
      if (state.buffer)   { wl_buffer_destroy(state.buffer);       state.buffer   = nullptr; }
      if (state.pool)     { wl_shm_pool_destroy(state.pool);       state.pool     = nullptr; }
      if (state.shm_data) { munmap(state.shm_data, state.shm_size); state.shm_data = nullptr; }
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

   if (!state.compositor || !state.shm || !state.wm_base)
      throw std::runtime_error("Missing required Wayland globals");

   // Build the surface → xdg_surface → xdg_toplevel chain
   state.surface  = wl_compositor_create_surface(state.compositor);
   state.xdg_surf = xdg_wm_base_get_xdg_surface(state.wm_base, state.surface);
   state.toplevel = xdg_surface_get_toplevel(state.xdg_surf);

   xdg_surface_add_listener(state.xdg_surf, &xdg_surface_lst, &state);
   xdg_toplevel_add_listener(state.toplevel, &toplevel_listener, &state);
   xdg_toplevel_set_title(state.toplevel, "Artist");
   xdg_toplevel_set_app_id(state.toplevel, "org.cycfi.artist");

   // Request server-side decorations (title bar, close button, etc.).
   // Falls back gracefully if the compositor doesn't support the protocol.
   if (state.deco_manager)
   {
      auto* deco = zxdg_decoration_manager_v1_get_toplevel_decoration(
         state.deco_manager, state.toplevel);
      zxdg_toplevel_decoration_v1_set_mode(
         deco, ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
   }

   // Initial commit triggers the configure event
   wl_surface_commit(state.surface);
   wl_display_roundtrip(state.display);

   create_buffer(state);

   if (animate)
   {
      // Kick off the frame-callback driven loop
      auto* cb = wl_surface_frame(state.surface);
      wl_callback_add_listener(cb, &frame_listener, &state);
   }

   render(state);

   while (state.running && wl_display_dispatch(state.display) != -1)
   {
      // Event loop — frame callbacks drive animation;
      // wl_display_dispatch blocks until the next event when idle.
   }

   // Tear down in reverse order
   if (state.buffer)     wl_buffer_destroy(state.buffer);
   if (state.pool)       wl_shm_pool_destroy(state.pool);
   if (state.shm_data)   munmap(state.shm_data, state.shm_size);
   if (state.shm_fd >= 0) close(state.shm_fd);
   if (state.toplevel)   xdg_toplevel_destroy(state.toplevel);
   if (state.xdg_surf)   xdg_surface_destroy(state.xdg_surf);
   if (state.surface)    wl_surface_destroy(state.surface);
   if (state.output)       wl_output_destroy(state.output);
   if (state.deco_manager) zxdg_decoration_manager_v1_destroy(state.deco_manager);
   if (state.wm_base)      xdg_wm_base_destroy(state.wm_base);
   if (state.shm)        wl_shm_destroy(state.shm);
   if (state.compositor) wl_compositor_destroy(state.compositor);
   wl_registry_destroy(state.registry);
   wl_display_disconnect(state.display);

   return 0;
}
