/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#if !defined(ARTIST_D2D_CONTEXT_APR_12_2020)
#define ARTIST_D2D_CONTEXT_APR_12_2020

#include <windows.h>
#include <artist/canvas.hpp>
#include <d2d1.h>
#include "d2d1_1.h"
#include <d2d1helper.h>
#include <dwrite.h>
#include <wincodec.h>
#include <memory>
#include <stdexcept>

namespace cycfi::artist::d2d
{
   using render_target        = ID2D1RenderTarget;
   using hwnd_render_target   = ID2D1HwndRenderTarget;
   using factory              = ID2D1Factory;
   using bitmap               = ID2D1Bitmap;
   using bitmap_render_target = ID2D1BitmapRenderTarget;
   using device_context       = ID2D1DeviceContext;

   ////////////////////////////////////////////////////////////////////////////
   // The main factory (singleton)
   ////////////////////////////////////////////////////////////////////////////
   factory& get_factory();

   ////////////////////////////////////////////////////////////////////////////
   // Abstract canvas state implementation
   ////////////////////////////////////////////////////////////////////////////
   struct context_state
   {
      virtual void         update(render_target& cnv) = 0;
      virtual void         discard() = 0;
   };

   ////////////////////////////////////////////////////////////////////////////
   // Main (low-level) canvas implementation
   ////////////////////////////////////////////////////////////////////////////
   struct context
   {
   public:
                              context(HWND hwnd, color bkd);
                              ~context();

                              template <typename Renderer>
      void                    render(Renderer&& draw);

      HWND                    hwnd() const;
      render_target*          target() const;
      void                    target(render_target* cnv);

      void                    state(context_state* state);
      context_state*          state() const;

   private:

                              context(context const&) = delete;
      context&                operator=(context const&) = delete;

      void                    update();
      void                    discard();

      HWND                    _hwnd = nullptr;
      render_target*          _target = nullptr;
      D2D1::ColorF            _bkd;
      context_state*          _state = nullptr;
   };

   struct offscreen_context
   {
                              offscreen_context(context& ctx);
                              offscreen_context(extent size, context& ctx);
                              ~offscreen_context();

      render_target*          target() const;
      device_context*         dc();
      d2d::bitmap*            bitmap();

   private:

      d2d::bitmap*            _bm = nullptr;
      bitmap_render_target*   _bm_target = nullptr;
      device_context*         _dc = nullptr;
      render_target*          _target = nullptr;
   };

   ////////////////////////////////////////////////////////////////////////////
   // Low-level D2D types
   ////////////////////////////////////////////////////////////////////////////
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
   // Low-level utilities
   ////////////////////////////////////////////////////////////////////////////
   template <typename Interface>
   inline void release(Interface*& ptr);

   brush* make_paint(color c, render_target& cn);
   brush* make_paint(canvas::linear_gradient const& lg, render_target& target);
   brush* make_paint(canvas::radial_gradient const& rg, render_target& target);

   template <typename Container>
   geometry_group* make_group(Container const& c);

   ////////////////////////////////////////////////////////////////////////////
   // Inlines
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

   template <typename Container>
   inline geometry_group* make_group(Container const& c, fill_mode mode)
   {
      geometry_group* group = nullptr;
      auto hr = get_factory().CreateGeometryGroup(
         mode
       , const_cast<geometry**>(c.data())
       , c.size()
       , &group
      );
      if (!SUCCEEDED(hr))
         throw std::runtime_error{ "Error: CreateGeometryGroup Fail." };
      return group;
   }

   inline context::context(HWND hwnd, color bkd)
    : _hwnd{ hwnd }
    , _bkd{ bkd.red, bkd.green, bkd.blue, bkd.alpha }
   {
   }

   inline context::~context()
   {
      release(_target);
   }

   inline HWND context::hwnd() const
   {
      return _hwnd;
   }

   inline render_target* context::target() const
   {
      return _target;
   };

   inline void context::target(render_target* cnv)
   {
      _target = cnv;
   }

   template <typename Renderer>
   void context::render(Renderer&& draw)
   {
      update();

      // $$ for now $$$ need dynamic cast if (!(_d2d_canvas->CheckWindowState() & D2D1_WINDOW_STATE_OCCLUDED))
      {
         _target->BeginDraw();
         _target->SetTransform(D2D1::Matrix3x2F::Identity());

         if (_bkd.a > 0)
            _target->Clear(_bkd);
         draw(*_target);

         auto hr = _target->EndDraw();
         if (hr == D2DERR_RECREATE_TARGET)
            discard();
      }
   }

   inline void context::discard()
   {
      if (_state)
         _state->discard();
      release(_target);
   }

   inline void context::state(context_state* state)
   {
      _state = state;
   }

   inline context_state* context::state() const
   {
      return _state;
   }

   inline offscreen_context::offscreen_context(context& ctx)
    : _target(ctx.target())
   {
      _target->CreateCompatibleRenderTarget(&_bm_target);
   }

   inline offscreen_context::offscreen_context(extent size, context& ctx)
    : _target(ctx.target())
   {
      _target->CreateCompatibleRenderTarget(
         { size.x, size.y }, &_bm_target
      );
   }

   inline offscreen_context::~offscreen_context()
   {
      release(_bm);
      release(_bm_target);
   }

   inline render_target* offscreen_context::target() const
   {
      return _bm_target;
   }

   inline device_context* offscreen_context::dc()
   {
      if (!_dc)
         _target->QueryInterface(&_dc);
      return _dc;
   }

   inline bitmap* offscreen_context::bitmap()
   {
      if (!_bm)
         _bm_target->GetBitmap(&_bm);
      return _bm;
   }
}

#endif
