/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include "context.hpp"
#include <algorithm>
#include <vector>

namespace cycfi::artist::d2d
{
   // The factories live as long as the process. Released from a static
   // destructor, they would be released while their library unloads, and
   // with two plugins in one host that faults in ntdll at exit. The
   // DirectWrite factory below is kept the same way.
   namespace detail
   {
      struct factory_maker
      {
         factory_maker()
         {
            // Create an ID2D1Factory1 (D2D 1.1). Render targets made from a
            // 1.1 factory can be QueryInterface'd to ID2D1DeviceContext, which
            // is what the shadow (Gaussian blur) and composite-op paths need.
            D2D1_FACTORY_OPTIONS opts{};
            D2D1CreateFactory(
               D2D1_FACTORY_TYPE_MULTI_THREADED,
               __uuidof(ID2D1Factory1), &opts,
               reinterpret_cast<void**>(&ptr)
            );
         }

         factory* ptr = nullptr;   // actually an ID2D1Factory1 (base-typed)
      };

      struct wic_factory_maker
      {
         wic_factory_maker()
         {
            CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            CoCreateInstance(
               CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
               IID_PPV_ARGS(&ptr)
            );
         }

         IWICImagingFactory* ptr = nullptr;
      };
   }

   factory& get_factory()
   {
      static detail::factory_maker maker;
      return *maker.ptr;
   }

   IWICImagingFactory& get_wic_factory()
   {
      static detail::wic_factory_maker maker;
      return *maker.ptr;
   }

   IDWriteFactory* dwrite_factory()
   {
      static IDWriteFactory* f = []
      {
         IDWriteFactory* p = nullptr;
         DWriteCreateFactory(
            DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
            reinterpret_cast<IUnknown**>(&p)
         );
         return p;
      }();
      return f;
   }

   ////////////////////////////////////////////////////////////////////////////
   // Brushes
   ////////////////////////////////////////////////////////////////////////////
   brush* make_paint(color c, render_target& cnv)
   {
      solid_color_brush* ptr = nullptr;
      auto hr = cnv.CreateSolidColorBrush(
         D2D1::ColorF(c.red, c.green, c.blue, c.alpha), &ptr
      );
      if (!SUCCEEDED(hr))
         throw std::runtime_error{"Error: CreateSolidColorBrush Fail."};
      return ptr;
   }

   namespace
   {
      gradient_stop_collection*
      make_stops(canvas::gradient const& g, render_target& cnv, float start = 0)
      {
         // start compresses the stops into [start, 1]: see the radial
         // make_paint below.
         std::vector<gradient_stop> stops;
         for (auto const& space : g.color_space)
            stops.push_back(
               {
                  start + space.offset * (1 - start),
                  {space.color.red, space.color.green, space.color.blue, space.color.alpha}
               }
            );

         gradient_stop_collection* collection = nullptr;
         auto hr = cnv.CreateGradientStopCollection(
            stops.data(), UINT32(stops.size()),
            D2D1_GAMMA_2_2, D2D1_EXTEND_MODE_CLAMP, &collection
         );
         if (!SUCCEEDED(hr))
            throw std::runtime_error{"Error: CreateGradientStopCollection Fail."};
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
         stops, &result
      );
      release(stops);
      if (!SUCCEEDED(hr))
         throw std::runtime_error{"Error: CreateLinearGradientBrush Fail."};
      return result;
   }

   float radial_start(canvas::radial_gradient const& rg)
   {
      return rg.c2_radius > 0?
         std::clamp(rg.c1_radius / rg.c2_radius, 0.0f, 1.0f) : 0.0f;
   }

   brush* make_paint(canvas::radial_gradient const& rg, render_target& target)
   {
      // Direct2D's radial brush has no inner circle: its ramp starts at the
      // origin. Running the stops from r1/r2 to 1 instead, with the clamp
      // holding the first stop inside, gives the first circle its own color.
      // This is exact for concentric circles.
      auto stops = make_stops(rg, target, radial_start(rg));
      radial_gradient_brush* result = nullptr;
      auto hr = target.CreateRadialGradientBrush(
         D2D1::RadialGradientBrushProperties(
            D2D1::Point2F(rg.c2.x, rg.c2.y),
            D2D1::Point2F(rg.c1.x - rg.c2.x, rg.c1.y - rg.c2.y),
            rg.c2_radius, rg.c2_radius
         ),
         stops, &result
      );
      release(stops);
      if (!SUCCEEDED(hr))
         throw std::runtime_error{"Error: CreateRadialGradientBrush Fail."};
      return result;
   }

   ////////////////////////////////////////////////////////////////////////////
   // offscreen_context
   ////////////////////////////////////////////////////////////////////////////
   offscreen_context::offscreen_context(context& ctx)
   {
      ctx.target()->CreateCompatibleRenderTarget(&_bm_target);
   }

   offscreen_context::offscreen_context(extent size, context& ctx)
   {
      ctx.target()->CreateCompatibleRenderTarget(
         D2D1::SizeF(size.x, size.y), &_bm_target
      );
   }

   offscreen_context::~offscreen_context()
   {
      release(_dc);
      release(_bm);
      release(_bm_target);
   }

   device_context* offscreen_context::dc()
   {
      if (!_dc)
         _bm_target->QueryInterface(&_dc);
      return _dc;
   }

   bitmap* offscreen_context::bitmap()
   {
      if (!_bm)
         _bm_target->GetBitmap(&_bm);
      return _bm;
   }
}
