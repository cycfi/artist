/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <context.hpp>

namespace cycfi::artist::d2d
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

         factory* ptr = nullptr;
      };
   }

   factory& get_factory()
   {
      static detail::factory_maker maker;
      return *maker.ptr;
   }

   void context::update()
   {
      if (!_target)
      {
         RECT rc;
         GetClientRect(_hwnd, &rc);

         D2D1_SIZE_U size = D2D1::SizeU(
            rc.right - rc.left,
            rc.bottom - rc.top
         );

         // Create a Direct2D render target.
         ID2D1HwndRenderTarget* target;
         auto hr = get_factory().CreateHwndRenderTarget(
            D2D1::RenderTargetProperties(),
            D2D1::HwndRenderTargetProperties(_hwnd, size),
            &target
         );
         _target = target;

         if (!SUCCEEDED(hr))
            throw std::runtime_error{ "Error: Failed to create RenderTarget." };
      }

      if (_state)
         _state->update(*_target);
   }

   brush* make_paint(color c, render_target& cnv)
   {
      solid_color_brush* ptr = nullptr;
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
      gradient_stop_collection*
      make_stops(canvas::gradient const& g, render_target& cnv)
      {
         std::vector<gradient_stop> stops;
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

         gradient_stop_collection* collection = nullptr;
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

   brush* make_paint(canvas::linear_gradient const& lg, render_target& target)
   {
      auto stops = make_stops(lg, target);

      linear_gradient_brush* result = nullptr;
      auto hr = target.CreateLinearGradientBrush(
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

   brush* make_paint(canvas::radial_gradient const& rg, render_target& target)
   {
      auto stops = make_stops(rg, target);

      radial_gradient_brush* result = nullptr;
      auto hr = target.CreateRadialGradientBrush(
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

