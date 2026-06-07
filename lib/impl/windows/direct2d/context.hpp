/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]

   Direct2D backend low-level context. Ported from the 2020 `direct2d` branch
   and adapted to the current `canvas_impl` seam: `canvas_impl` == `d2d::context`,
   a thin holder of the active render target + canvas state. The HWND render
   loop and device-loss recovery live in the host (examples/host/windows/
   direct2d_app.cpp); the headless WIC offscreen path lives in image.cpp.
=============================================================================*/
#if !defined(ARTIST_D2D_CONTEXT_JUNE_7_2026)
#define ARTIST_D2D_CONTEXT_JUNE_7_2026

#include <windows.h>
#include <artist/canvas.hpp>
#include <d2d1.h>
#include <d2d1_1.h>
#include <d2d1helper.h>
#include <dwrite.h>
#include <wincodec.h>
#include <memory>
#include <stdexcept>

namespace cycfi::artist::d2d
{
   ////////////////////////////////////////////////////////////////////////////
   // Low-level D2D type aliases
   ////////////////////////////////////////////////////////////////////////////
   using render_target                 = ID2D1RenderTarget;
   using hwnd_render_target            = ID2D1HwndRenderTarget;
   using factory                       = ID2D1Factory;
   using bitmap                        = ID2D1Bitmap;
   using bitmap_render_target          = ID2D1BitmapRenderTarget;
   using device_context                = ID2D1DeviceContext;

   using brush                         = ID2D1Brush;
   using solid_color_brush             = ID2D1SolidColorBrush;
   using linear_gradient_brush         = ID2D1LinearGradientBrush;
   using radial_gradient_brush         = ID2D1RadialGradientBrush;
   using geometry                      = ID2D1Geometry;
   using geometry_group                = ID2D1GeometryGroup;
   using fill_mode                     = D2D1_FILL_MODE;
   using rect_geometry                 = ID2D1RectangleGeometry;
   using round_rect_geometry           = ID2D1RoundedRectangleGeometry;
   using ellipse_geometry              = ID2D1EllipseGeometry;
   using path_geometry                 = ID2D1PathGeometry;
   using geometry_sink                 = ID2D1GeometrySink;
   using figure_begin                  = D2D1_FIGURE_BEGIN;
   using arc_segment                   = D2D1_ARC_SEGMENT;
   using quadratic_bezier_segment      = D2D1_QUADRATIC_BEZIER_SEGMENT;
   using bezier_segment                = D2D1_BEZIER_SEGMENT;
   using stroke_style                  = ID2D1StrokeStyle;
   using stroke_style_properties       = D2D1_STROKE_STYLE_PROPERTIES;
   using cap_style                     = D2D1_CAP_STYLE;
   using line_join                     = D2D1_LINE_JOIN;
   using gradient_stop                 = D2D1_GRADIENT_STOP;
   using gradient_stop_collection      = ID2D1GradientStopCollection;
   using matrix2x2f                    = D2D1::Matrix3x2F;
   using rectf                         = D2D1_RECT_F;
   using effect                        = ID2D1Effect;

   constexpr auto sweep_dir_ccw        = D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE;
   constexpr auto sweep_dir_cw         = D2D1_SWEEP_DIRECTION_CLOCKWISE;
   constexpr auto arc_small            = D2D1_ARC_SIZE_SMALL;
   constexpr auto arc_large            = D2D1_ARC_SIZE_LARGE;
   constexpr auto figure_path_open     = D2D1_FIGURE_END_OPEN;
   constexpr auto figure_end_closed    = D2D1_FIGURE_END_CLOSED;
   constexpr auto figure_begin_hollow  = D2D1_FIGURE_BEGIN_HOLLOW;
   constexpr auto figure_begin_filled  = D2D1_FIGURE_BEGIN_FILLED;

   constexpr auto line_join_miter      = D2D1_LINE_JOIN_MITER;
   constexpr auto line_join_bevel      = D2D1_LINE_JOIN_BEVEL;
   constexpr auto line_join_round      = D2D1_LINE_JOIN_ROUND;
   constexpr auto cap_style_flat       = D2D1_CAP_STYLE_FLAT;
   constexpr auto cap_style_square     = D2D1_CAP_STYLE_SQUARE;
   constexpr auto cap_style_round      = D2D1_CAP_STYLE_ROUND;

   constexpr auto fill_mode_winding    = D2D1_FILL_MODE_WINDING;
   constexpr auto fill_mode_alternate  = D2D1_FILL_MODE_ALTERNATE;

   ////////////////////////////////////////////////////////////////////////////
   // The main factory (singleton) and the shared WIC factory.
   ////////////////////////////////////////////////////////////////////////////
   factory&             get_factory();
   IWICImagingFactory&  get_wic_factory();

   ////////////////////////////////////////////////////////////////////////////
   // Abstract canvas state hook (so device-dependent resources held by the
   // canvas can be recreated/discarded on device loss).
   ////////////////////////////////////////////////////////////////////////////
   struct context_state
   {
      virtual void   update(render_target& cnv) = 0;
      virtual void   discard() = 0;
   };

   ////////////////////////////////////////////////////////////////////////////
   // context: the type behind `canvas_impl`. A thin holder of the active
   // render target plus the canvas state hook. Target lifetime/device-loss is
   // owned by whoever created the target (host or offscreen_image).
   ////////////////////////////////////////////////////////////////////////////
   struct context
   {
                        context() = default;
      explicit          context(render_target* target) : _target{target} {}

      render_target*    target() const            { return _target; }
      void              target(render_target* t)  { _target = t; }

      context_state*    state() const             { return _state; }
      void              state(context_state* s)   { _state = s; }

   private:

      render_target*    _target = nullptr;
      context_state*    _state = nullptr;
   };

   ////////////////////////////////////////////////////////////////////////////
   // offscreen_context: a compatible (in-memory) render target spun off the
   // active target. Used by the shadow-blur path.
   ////////////////////////////////////////////////////////////////////////////
   struct offscreen_context
   {
      explicit          offscreen_context(context& ctx);
                        offscreen_context(extent size, context& ctx);
                        ~offscreen_context();

      render_target*    target() const            { return _bm_target; }
      device_context*   dc();
      d2d::bitmap*      bitmap();

   private:

      d2d::bitmap*            _bm = nullptr;
      bitmap_render_target*   _bm_target = nullptr;
      device_context*         _dc = nullptr;
   };

   ////////////////////////////////////////////////////////////////////////////
   // Low-level utilities
   ////////////////////////////////////////////////////////////////////////////
   template <typename Interface>
   inline void release(Interface*& ptr)
   {
      if (ptr)
      {
         ptr->Release();
         ptr = nullptr;
      }
   }

   brush* make_paint(color c, render_target& cnv);
   brush* make_paint(canvas::linear_gradient const& lg, render_target& target);
   brush* make_paint(canvas::radial_gradient const& rg, render_target& target);

   // Access the backing WIC bitmap of an image (defined in image.cpp). Returns
   // nullptr for an empty image.
   IWICBitmap* wic_bitmap(image const& img);

   template <typename Container>
   inline geometry_group* make_group(Container const& c, fill_mode mode)
   {
      geometry_group* group = nullptr;
      auto hr = get_factory().CreateGeometryGroup(
         mode
       , const_cast<geometry**>(c.data())
       , UINT32(c.size())
       , &group
      );
      if (!SUCCEEDED(hr))
         throw std::runtime_error{"Error: CreateGeometryGroup Fail."};
      return group;
   }
}

#endif
