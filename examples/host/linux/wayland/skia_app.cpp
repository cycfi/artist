/*=============================================================================
   Copyright (c) 2016-2023 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include "../../../app.hpp"
#include <wayland-client.h>
#include <wayland-egl.h>
#include <libdecor.h>
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
#include <cstdlib>
#include <cstdio>
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
      wp_viewporter*                  viewporter   = nullptr;
      wp_fractional_scale_manager_v1* frac_manager = nullptr;

      // libdecor handles xdg-shell and decorations
      libdecor*                       decor        = nullptr;
      libdecor_frame*                 frame        = nullptr;
      wl_surface*                     surface      = nullptr;
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

      // Startup sequencing: GNOME emits scale=2, configure, scale=1 (spurious),
      // scale=2, configure before the window is stable. Track configure count
      // so we can ignore the spurious scale decrease during startup.
      int                             configure_count = 0;

      // First-resize workaround: libdecor on HiDPI GNOME reports the cursor grab
      // position as the content size on the very first resize configure. We
      // record the grab origin and apply a delta correction to all subsequent
      // events until the raw value grows past the pre-resize size.
      bool                            first_resize_done  = false;

      // Resize coalescing: a drag delivers a burst of configure events; we
      // record the latest requested size and apply it once per dispatch cycle
      // (in the main loop) rather than rendering every intermediate size.
      int                             pend_w       = 0;
      int                             pend_h       = 0;
      bool                            needs_resize = false;

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
      float new_scale = float(scale_120) / 120.0f;

      // GNOME emits: scale=2, configure, scale=1 (spurious), scale=2, configure.
      // Ignore scale decreases until the window has seen at least 2 configures.
      if (state.configure_count < 2 && new_scale < state.scale)
         return;

      state.scale = new_scale;
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
   // wl_registry
   void registry_global(void* data, wl_registry* registry,
                        uint32_t name, const char* interface, uint32_t /*version*/)
   {
      auto& state = *static_cast<app_state*>(data);

      if (strcmp(interface, wl_compositor_interface.name) == 0)
         state.compositor = static_cast<wl_compositor*>(
            wl_registry_bind(registry, name, &wl_compositor_interface, 4));
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
   // EGL: create window surface — deferred to first configure
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

      // Pre-fill both EGL buffers (front + back) with the app background color.
      // Without this, the compositor briefly shows the uninitialized buffer
      // contents before the first draw() call completes, causing a visible flicker.
      glClearColor(state.bkd.red, state.bkd.green, state.bkd.blue, 1.0f);
      for (int i = 0; i < 2; ++i)
      {
         glClear(GL_COLOR_BUFFER_BIT);
         eglSwapBuffers(state.egl_display, state.egl_surface);
      }
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

      // Tell the compositor to display this physical-pixel buffer at logical size.
      if (state.viewport)
         wp_viewport_set_destination(state.viewport,
            int(state.size.x), int(state.size.y));

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

   // -------------------------------------------------------------------------
   // libdecor frame callbacks
   void libdecor_frame_configure(
      libdecor_frame*         frame,
      libdecor_configuration* config,
      void*                   data)
   {
      auto& state = *static_cast<app_state*>(data);

      int w = int(state.size.x);
      int h = int(state.size.y);
      libdecor_configuration_get_content_size(config, frame, &w, &h);

      state.configure_count++;

      // First configure: create EGL window + Skia + do the initial render inline.
      if (state.egl_window == nullptr)
      {
         state.size = {float(w), float(h)};
         auto* st = libdecor_state_new(w, h);
         libdecor_frame_commit(frame, st, config);
         libdecor_state_free(st);
         init_egl_window(state);
         init_skia(state);
         create_skia_surface(state);
         render(state);
         state.configured = true;
         return;
      }

      // Known GNOME 46 HiDPI bug: libdecor reports the cursor grab position
      // (roughly half the window size) as the content size on the very first
      // resize configure. Detect it, ack with the current size so the frame
      // doesn't shrink, and skip the resize. Subsequent events are normal.
      // Only consume the guard on a genuine size change — startup emits a
      // same-size re-configure that must not trigger the guard.
      if (!state.first_resize_done)
      {
         if (w != int(state.size.x) || h != int(state.size.y))
         {
            state.first_resize_done = true;
            if (w < int(state.size.x * 0.6f))
            {
               auto* st = libdecor_state_new(int(state.size.x), int(state.size.y));
               libdecor_frame_commit(frame, st, config);
               libdecor_state_free(st);
               return;
            }
         }
      }

      auto* st = libdecor_state_new(w, h);
      libdecor_frame_commit(frame, st, config);
      libdecor_state_free(st);

      state.pend_w = w;
      state.pend_h = h;
      state.needs_resize = true;
   }

   void libdecor_frame_close(libdecor_frame* /*frame*/, void* data)
   {
      static_cast<app_state*>(data)->running = false;
   }

   void libdecor_frame_commit(libdecor_frame* /*frame*/, void* /*data*/) {}

   libdecor_frame_interface frame_iface = {
      .configure = libdecor_frame_configure,
      .close     = libdecor_frame_close,
      .commit    = libdecor_frame_commit,
   };

   // -------------------------------------------------------------------------
   // libdecor error handler
   void libdecor_error(libdecor* /*ctx*/, libdecor_error /*err*/, const char* msg)
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
   wl_display_roundtrip(state.display);
   wl_display_roundtrip(state.display);

   if (!state.compositor)
      throw std::runtime_error("Missing required Wayland globals");

   // EGL display + config + context (window surface deferred to configure)
   init_egl(state);

   // Surface + viewport + fractional scale.
   // We use wp_viewport_set_destination to tell the compositor the logical
   // display size; wl_surface_set_buffer_scale is unreliable with EGL surfaces.
   state.surface = wl_compositor_create_surface(state.compositor);
   if (state.viewporter)
      state.viewport = wp_viewporter_get_viewport(state.viewporter, state.surface);
   if (state.frac_manager)
   {
      state.frac_scale = wp_fractional_scale_manager_v1_get_fractional_scale(
         state.frac_manager, state.surface);
      wp_fractional_scale_v1_add_listener(
         state.frac_scale, &frac_scale_listener, &state);
   }

   // libdecor window
   state.decor = libdecor_new(state.display, &decor_iface);
   state.frame = libdecor_decorate(state.decor, state.surface, &frame_iface, &state);

   libdecor_frame_set_title(state.frame, "Artist (wayland skia)");
   libdecor_frame_set_app_id(state.frame, "org.cycfi.artist");
   libdecor_frame_set_min_content_size(state.frame, 100, 100);
   libdecor_frame_map(state.frame);

   // Pump until configure
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

      // Apply the coalesced resize: a drag delivers many configure events in a
      // single dispatch; render only the final size once, so the window tracks
      // the cursor instead of lagging through intermediate sizes.
      if (state.needs_resize)
      {
         state.needs_resize = false;
         if (int(state.size.x) != state.pend_w || int(state.size.y) != state.pend_h)
         {
            state.size = {float(state.pend_w), float(state.pend_h)};
            create_skia_surface(state);
            render(state);
         }
      }
   }

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
   if (state.viewport)      wp_viewport_destroy(state.viewport);
   if (state.frac_scale)    wp_fractional_scale_v1_destroy(state.frac_scale);
   if (state.frac_manager)  wp_fractional_scale_manager_v1_destroy(state.frac_manager);
   libdecor_frame_unref(state.frame);
   libdecor_unref(state.decor);
   if (state.surface)       wl_surface_destroy(state.surface);
   if (state.compositor)    wl_compositor_destroy(state.compositor);
   wl_registry_destroy(state.registry);
   wl_display_disconnect(state.display);

   return 0;
}
