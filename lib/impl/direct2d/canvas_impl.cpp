/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <canvas_impl.hpp>

namespace cycfi::artist
{
   namespace detail
   {
      struct factory_maker
      {
         factory_maker()
         {
            D2D1CreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED, &ptr);
         }

         ~factory_maker()
         {
            release(ptr);
         }

         d2d_factory* ptr = nullptr;
      };
   }

   d2d_factory& get_factory()
   {
      static detail::factory_maker maker;
      return *maker.ptr;
   }

   void canvas_impl::update()
   {
      if (!_d2d_canvas)
      {
         RECT rc;
         GetClientRect(_hwnd, &rc);

         D2D1_SIZE_U size = D2D1::SizeU(
            rc.right - rc.left,
            rc.bottom - rc.top
         );

         // Create a Direct2D render target.
         auto hr = get_factory().CreateHwndRenderTarget(
            D2D1::RenderTargetProperties(),
            D2D1::HwndRenderTargetProperties(_hwnd, size),
            &_d2d_canvas
         );

         if (!SUCCEEDED(hr))
            throw std::runtime_error{ "Error: Failed to create RenderTarget." };
      }

      if (_state)
         _state->update(*_d2d_canvas);
   }

   d2d_paint* make_paint(color c, d2d_canvas& cnv)
   {
      d2d_solid_color* ptr = nullptr;
      auto hr = cnv.CreateSolidColorBrush(
         D2D1::ColorF(c.red, c.green, c.blue, c.alpha)
       , &ptr
      );
      if (!SUCCEEDED(hr))
         throw std::runtime_error{ "Error: CreateSolidColorBrush Fail." };
      return ptr;
   }

   namespace
   {
      d2d_gradient_stop_collection*
      make_stops(canvas::gradient const& g, d2d_canvas& cnv)
      {
         std::vector<d2d_gradient_stop> stops;
         for (auto const& space : g.color_space)
            stops.push_back(
               {
                  space.offset,
                  {
                     space.color.red,
                     space.color.green,
                     space.color.blue,
                     space.color.alpha
                  }
               }
            );

         d2d_gradient_stop_collection* collection = nullptr;
         auto hr = cnv.CreateGradientStopCollection(
            stops.data(),
            stops.size(),
            D2D1_GAMMA_2_2,
            D2D1_EXTEND_MODE_CLAMP,
            &collection
         );

         if (!SUCCEEDED(hr))
            throw std::runtime_error{ "Error: CreateGradientStopCollection Fail." };
         return collection;
      }
   }

   d2d_paint* make_paint(canvas::linear_gradient const& lg, d2d_canvas& cnv)
   {
      auto stops = make_stops(lg, cnv);

      d2d_linear_gradient* result = nullptr;
      auto hr = cnv.CreateLinearGradientBrush(
         D2D1::LinearGradientBrushProperties(
            D2D1::Point2F(lg.start.x, lg.start.y),
            D2D1::Point2F(lg.end.x, lg.end.y)
         ),
         stops,
         &result
      );
      if (!SUCCEEDED(hr))
         throw std::runtime_error{ "Error: CreateLinearGradientBrush Fail." };
      return result;
   }

   d2d_paint* make_paint(canvas::radial_gradient const& rg, d2d_canvas& cnv)
   {
      auto stops = make_stops(rg, cnv);

      d2d_radial_gradient* result = nullptr;
      auto hr = cnv.CreateRadialGradientBrush(
        D2D1::RadialGradientBrushProperties(
            D2D1::Point2F(rg.c1.x, rg.c1.y),
            D2D1::Point2F(rg.c2.x-rg.c1.x, rg.c2.y-rg.c1.y),
            rg.c2_radius,
            rg.c2_radius
         ),
         stops,
         &result
      );
      if (!SUCCEEDED(hr))
         throw std::runtime_error{ "Error: CreateRadialGradientBrush Fail." };
      return result;
   }
}

