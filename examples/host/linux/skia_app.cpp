/*=============================================================================
   Copyright (c) 2016-2023 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include "../../app.hpp"
#include <wayland-client.h>
#include <wayland-egl.h>
#include "xdg-shell-client-protocol.h"
#include "xdg-decoration-unstable-v1-client-protocol.h"
#include "fractional-scale-v1-client-protocol.h"
#include "viewporter-client-protocol.h"
#include <EGL/egl.h>
#include <GL/gl.h>

#include <SkImage.h>
#include <SkColorSpace.h>
#include <SkCanvas.h>
#include <SkSurface.h>
#include <ganesh/GrDirectContext.h>
#include <ganesh/GrBackendSurface.h>
#include <ganesh/SkSurfaceGanesh.h>
#include <ganesh/gl/GrGLInterface.h>
#include <ganesh/gl/GrGLDirectContext.h>
#include <ganesh/gl/GrGLBackendSurface.h>
#include <ganesh/gl/GrGLTypes.h>
#include <ganesh/gl/egl/GrGLMakeEGLInterface.h>

#include <chrono>
#include <cstring>
#include <cmath>
#include <stdexcept>

using namespace cycfi::artist;
float elapsed_ = 0;  // rendering elapsed time

namespace
{
   struct app_state
   {
      // Wayland globals
      wl_display*                     display      = nullptr;
      wl_registry*                    registry     = nullptr;
      wl_compositor*                  compositor   = nullptr;
      xdg_wm_base*                    wm_base      = nullptr;
      zxdg_decoration_manager_v1*     deco_manager = nullptr;
      wp_viewporter*                  viewporter   = nullptr;
      wp_fractional_scale_manager_v1* frac_manager = nullptr;

      // Surfaces
      wl_surface*                     surface      = nullptr;
      xdg_surface*                    xdg_surf     = nullptr;
      xdg_toplevel*                   toplevel     = nullptr;
      wp_viewport*                    viewport     = nullptr;
      wp_fractional_scale_v1*         frac_scale   = nullptr;

      // EGL
      EGLDisplay                      egl_display  = EGL_NO_DISPLAY;
      EGLContext                      egl_context  = EGL_NO_CONTEXT;
      EGLSurface                      egl_surface  = EGL_NO_SURFACE;
      EGLConfig                       egl_config   = nullptr;
      wl_egl_window*                  egl_window   = nullptr;

      // Skia
      sk_sp<const GrGLInterface>      xface;
      sk_sp<GrDirectContext>          ctx;
      sk_sp<SkSurface>                skia_surface;

      // App parameters
      extent                          size         = {};
      float                           scale        = 1.0f;
      color                           bkd          = colors::white;
      bool                            animate      = false;

      // State flags
      bool                            running      = true;
      bool                            configured   = false;

      // Frame timing (throttle)
      uint32_t                        last_frame_ms  = 0;
      uint32_t                        frame_interval = 1000/60;
   };

   // -------------------------------------------------------------------------
   // Forward declarations
   void frame_done(void* data, wl_callback* cb, uint32_t time);
   void init_egl_window(app_state& state);
   void init_skia(app_state& state);
   void create_skia_surface(app_state& state);
   void render(app_state& state);

   constexpr wl_callback_listener frame_listener = {
      .done = frame_done
   };

   // -------------------------------------------------------------------------
   // Render via Skia Ganesh GL
   void render(app_state& state)
   {
      if (!state.configured || !state.skia_surface)
         return;

      auto start = std::chrono::steady_clock::now();

      SkCanvas* gpu_canvas = state.skia_surface->getCanvas();
      gpu_canvas->save();
      gpu_canvas->scale(state.scale, state.scale);
      auto cnv = canvas{gpu_canvas};
      draw(cnv);
      gpu_canvas->restore();
      state.ctx->flushAndSubmit(state.skia_surface.get());

      auto stop = std::chrono::steady_clock::now();
      elapsed_ = std::chrono::duration<double>{stop - start}.count();

      if (state.viewport)
         wp_viewport_set_destination(state.viewport,
            int(state.size.x), int(state.size.y));

      eglSwapBuffers(state.egl_display, state.egl_surface);
   }

   // -------------------------------------------------------------------------
   // Frame callback — drives animation loop
   void frame_done(void* data, wl_callback* cb, uint32_t time)
   {
      wl_callback_destroy(cb);
      auto& state = *static_cast<app_state*>(data);

      auto* next = wl_surface_frame(state.surface);
      wl_callback_add_listener(next, &frame_listener, &state);

      uint32_t const delta = time - state.last_frame_ms;
      if (state.last_frame_ms != 0 && delta < state.frame_interval)
      {
         eglSwapBuffers(state.egl_display, state.egl_surface);
         return;
      }

      state.last_frame_ms = time;
      render(state);
   }

   // -------------------------------------------------------------------------
   // wp_fractional_scale_v1 listener
   void frac_scale_preferred(void* data, wp_fractional_scale_v1* /*frac*/,
                             uint32_t scale_120)
   {
      auto& state = *static_cast<app_state*>(data);
      state.scale = float(scale_120) / 120.0f;
      if (state.configured)
      {
         create_skia_surface(state);
         render(state);
      }
   }

   constexpr wp_fractional_scale_v1_listener frac_scale_listener = {
      .preferred_scale = frac_scale_preferred
   };

   // -------------------------------------------------------------------------
   // xdg_wm_base — must respond to ping
   void wm_base_ping(void* /*data*/, xdg_wm_base* wm_base, uint32_t serial)
   {
      xdg_wm_base_pong(wm_base, serial);
   }

   constexpr xdg_wm_base_listener wm_base_listener = {
      .ping = wm_base_ping
   };

   // -------------------------------------------------------------------------
   // xdg_surface — ack configure then do first render
   void xdg_surface_configure(void* data, xdg_surface* xdg_surf, uint32_t serial)
   {
      auto& state = *static_cast<app_state*>(data);
      xdg_surface_ack_configure(xdg_surf, serial);

      // Create EGL window + Skia on first configure, after xdg-surface is
      // fully set up so server-side decorations are guaranteed to apply.
      if (state.egl_window == nullptr)
      {
         init_egl_window(state);
         init_skia(state);
         create_skia_surface(state);
      }

      state.configured = true;
      render(state);
   }

   constexpr xdg_surface_listener xdg_surface_lst = {
      .configure = xdg_surface_configure
   };

   // -------------------------------------------------------------------------
   // xdg_toplevel
   void toplevel_configure(void* /*data*/, xdg_toplevel*,
                           int32_t /*w*/, int32_t /*h*/, wl_array*) {}

   void toplevel_close(void* data, xdg_toplevel*)
   {
      static_cast<app_state*>(data)->running = false;
   }

   constexpr xdg_toplevel_listener toplevel_listener = {
      .configure = toplevel_configure,
      .close     = toplevel_close
   };

   // -------------------------------------------------------------------------
   // wl_registry
   void registry_global(void* data, wl_registry* registry,
                        uint32_t name, const char* interface, uint32_t /*version*/)
   {
      auto& state = *static_cast<app_state*>(data);

      if (strcmp(interface, wl_compositor_interface.name) == 0)
         state.compositor = static_cast<wl_compositor*>(
            wl_registry_bind(registry, name, &wl_compositor_interface, 4));
      else if (strcmp(interface, xdg_wm_base_interface.name) == 0)
      {
         state.wm_base = static_cast<xdg_wm_base*>(
            wl_registry_bind(registry, name, &xdg_wm_base_interface, 1));
         xdg_wm_base_add_listener(state.wm_base, &wm_base_listener, nullptr);
      }
      else if (strcmp(interface, zxdg_decoration_manager_v1_interface.name) == 0)
         state.deco_manager = static_cast<zxdg_decoration_manager_v1*>(
            wl_registry_bind(registry, name, &zxdg_decoration_manager_v1_interface, 1));
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
   // EGL: initialise display + config + context
   void init_egl(app_state& state)
   {
      state.egl_display = eglGetDisplay((EGLNativeDisplayType)state.display);
      if (state.egl_display == EGL_NO_DISPLAY)
         throw std::runtime_error("eglGetDisplay failed");
      if (!eglInitialize(state.egl_display, nullptr, nullptr))
         throw std::runtime_error("eglInitialize failed");

      EGLint config_attribs[] = {
         EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
         EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
         EGL_RED_SIZE,   8,
         EGL_GREEN_SIZE, 8,
         EGL_BLUE_SIZE,  8,
         EGL_ALPHA_SIZE, 8,
         EGL_NONE
      };
      EGLint num_configs = 0;
      if (!eglChooseConfig(state.egl_display, config_attribs, &state.egl_config, 1, &num_configs)
            || num_configs == 0)
         throw std::runtime_error("eglChooseConfig failed");

      eglBindAPI(EGL_OPENGL_API);
      EGLint ctx_attribs[] = { EGL_NONE };
      state.egl_context = eglCreateContext(
         state.egl_display, state.egl_config, EGL_NO_CONTEXT, ctx_attribs);
      if (state.egl_context == EGL_NO_CONTEXT)
         throw std::runtime_error("eglCreateContext failed");
   }

   // -------------------------------------------------------------------------
   // EGL: create window surface — deferred to after xdg-surface is configured
   void init_egl_window(app_state& state)
   {
      int const w = int(std::ceil(state.size.x * state.scale));
      int const h = int(std::ceil(state.size.y * state.scale));
      state.egl_window  = wl_egl_window_create(state.surface, w, h);
      state.egl_surface = eglCreateWindowSurface(
         state.egl_display, state.egl_config,
         (EGLNativeWindowType)state.egl_window, nullptr);
      if (state.egl_surface == EGL_NO_SURFACE)
         throw std::runtime_error("eglCreateWindowSurface failed");
      if (!eglMakeCurrent(state.egl_display, state.egl_surface,
                          state.egl_surface, state.egl_context))
         throw std::runtime_error("eglMakeCurrent failed");
   }

   // -------------------------------------------------------------------------
   // Skia initialisation
   void init_skia(app_state& state)
   {
      state.xface = GrGLMakeNativeInterface();
      if (!state.xface)
         state.xface = GrGLInterfaces::MakeEGL();
      if (!state.xface)
         throw std::runtime_error("GrGLMakeNativeInterface / MakeEGL failed");
      state.ctx = GrDirectContexts::MakeGL(state.xface);
      if (!state.ctx)
         throw std::runtime_error("GrDirectContexts::MakeGL failed");
   }

   // -------------------------------------------------------------------------
   // Create (or recreate) the Skia render surface at current physical size
   void create_skia_surface(app_state& state)
   {
      state.skia_surface.reset();

      int const w = int(std::ceil(state.size.x * state.scale));
      int const h = int(std::ceil(state.size.y * state.scale));

      wl_egl_window_resize(state.egl_window, w, h, 0, 0);

      GrGLint buffer = 0;
      glGetIntegerv(GL_FRAMEBUFFER_BINDING, &buffer);
      GrGLFramebufferInfo info;
      info.fFBOID  = (GrGLuint)buffer;
      info.fFormat = GL_RGBA8;

      auto target = GrBackendRenderTargets::MakeGL(w, h, 0, 8, info);
      state.skia_surface = SkSurfaces::WrapBackendRenderTarget(
         state.ctx.get(), target,
         kBottomLeft_GrSurfaceOrigin, kRGBA_8888_SkColorType,
         nullptr, nullptr
      );
      if (!state.skia_surface)
         throw std::runtime_error("SkSurfaces::WrapBackendRenderTarget failed");
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
   wl_display_roundtrip(state.display);
   wl_display_roundtrip(state.display);

   if (!state.compositor || !state.wm_base)
      throw std::runtime_error("Missing required Wayland globals");

   // EGL display + config + context (window surface deferred to configure)
   init_egl(state);

   // Surface chain
   state.surface  = wl_compositor_create_surface(state.compositor);

   // Fractional scale + viewport for HiDPI
   if (state.frac_manager && state.viewporter)
   {
      state.frac_scale = wp_fractional_scale_manager_v1_get_fractional_scale(
         state.frac_manager, state.surface);
      wp_fractional_scale_v1_add_listener(
         state.frac_scale, &frac_scale_listener, &state);
      state.viewport = wp_viewporter_get_viewport(state.viewporter, state.surface);
   }

   state.xdg_surf = xdg_wm_base_get_xdg_surface(state.wm_base, state.surface);
   state.toplevel = xdg_surface_get_toplevel(state.xdg_surf);

   xdg_surface_add_listener(state.xdg_surf, &xdg_surface_lst, &state);
   xdg_toplevel_add_listener(state.toplevel, &toplevel_listener, &state);

   xdg_toplevel_set_title(state.toplevel, "Artist");
   xdg_toplevel_set_app_id(state.toplevel, "org.cycfi.artist");

   // Fixed-size window: remove resize and maximize
   xdg_toplevel_set_min_size(state.toplevel, int(state.size.x), int(state.size.y));
   xdg_toplevel_set_max_size(state.toplevel, int(state.size.x), int(state.size.y));

   // Request server-side decorations (title bar, close/minimise buttons).
   // GNOME 43+ supports this; falls back gracefully if unavailable.
   if (state.deco_manager)
   {
      auto* deco = zxdg_decoration_manager_v1_get_toplevel_decoration(
         state.deco_manager, state.toplevel);
      zxdg_toplevel_decoration_v1_set_mode(
         deco, ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
   }

   // Commit to trigger the configure event
   wl_surface_commit(state.surface);
   wl_display_roundtrip(state.display);

   // Pump until fully configured
   while (!state.configured)
   {
      if (wl_display_dispatch(state.display) < 0)
         throw std::runtime_error("wl_display_dispatch failed during init");
   }

   if (animate)
   {
      auto* cb = wl_surface_frame(state.surface);
      wl_callback_add_listener(cb, &frame_listener, &state);
      render(state);
   }

   // Main event loop
   while (state.running && wl_display_dispatch(state.display) != -1)
   {}

   // Tear down
   state.skia_surface.reset();
   state.ctx.reset();
   state.xface.reset();
   if (state.egl_surface != EGL_NO_SURFACE)
      eglDestroySurface(state.egl_display, state.egl_surface);
   if (state.egl_context != EGL_NO_CONTEXT)
      eglDestroyContext(state.egl_display, state.egl_context);
   if (state.egl_window)
      wl_egl_window_destroy(state.egl_window);
   if (state.egl_display != EGL_NO_DISPLAY)
      eglTerminate(state.egl_display);
   if (state.frac_scale)    wp_fractional_scale_v1_destroy(state.frac_scale);
   if (state.viewport)      wp_viewport_destroy(state.viewport);
   if (state.frac_manager)  wp_fractional_scale_manager_v1_destroy(state.frac_manager);
   if (state.viewporter)    wp_viewporter_destroy(state.viewporter);
   if (state.toplevel)      xdg_toplevel_destroy(state.toplevel);
   if (state.xdg_surf)      xdg_surface_destroy(state.xdg_surf);
   if (state.deco_manager)  zxdg_decoration_manager_v1_destroy(state.deco_manager);
   if (state.wm_base)       xdg_wm_base_destroy(state.wm_base);
   if (state.surface)       wl_surface_destroy(state.surface);
   if (state.compositor)    wl_compositor_destroy(state.compositor);
   wl_registry_destroy(state.registry);
   wl_display_disconnect(state.display);

   return 0;
}
