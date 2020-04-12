/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#if !defined(ELEMENTS_DETAIL_DIRECT_2D_CANVAS_IMPL_APR_12_2020)
#define ELEMENTS_DETAIL_DIRECT_2D_CANVAS_IMPL_APR_12_2020

#include <windows.h>
#include <artist/canvas.hpp>
#include <d2d1.h>
#include <d2d1helper.h>
#include <dwrite.h>
#include <wincodec.h>
#include <variant>
#include <functional>
#include <stdexcept>

namespace cycfi::artist
{
   template <class Interface>
   inline void release(Interface*& ptr)
   {
      if (ptr != nullptr)
      {
         (ptr)->Release();
         ptr = nullptr;
      }
   }

   using d2d_render_target = ID2D1HwndRenderTarget;
   using d2d_factory = ID2D1Factory;

   // using d2d_color = ID2D1SolidColorBrush;
   // using d2d_linear_gradient = ID2D1LinearGradientBrush;
   // using d2d_radial_gradient = ID2D1RadialGradientBrush;

   // using fill_brush = std::variant<
   //    std::pair<color, d2d_color*>
   //  , std::pair<canvas::linear_gradient, d2d_linear_gradient*>
   //  , std::pair<canvas::radial_gradient, d2d_radial_gradient>
   // >;

   struct canvas_impl
   {
   public:
                           canvas_impl(HWND hwnd, d2d_factory* factory);
                           ~canvas_impl();

      bool                 update();
      void                 discard();
      HWND                 hwnd() const;
      d2d_render_target*   render_target() const;
      d2d_factory*         factory() const;

      using update_function = std::function<void(canvas_impl const&)>;

      update_function      on_update;

   private:

      HWND                 _hwnd = nullptr;
      d2d_factory*         _factory = nullptr;
      d2d_render_target*   _render_target = nullptr;
   };

   ////////////////////////////////////////////////////////////////////////////
   // Inlines
   ////////////////////////////////////////////////////////////////////////////
   inline canvas_impl::canvas_impl(HWND hwnd, d2d_factory* factory)
    : _factory{ factory }
    , _hwnd{ hwnd }
   {
   }

   inline canvas_impl::~canvas_impl()
   {
      release(_render_target);
   }

   inline HWND canvas_impl::hwnd() const
   {
      return _hwnd;
   }

   inline d2d_render_target* canvas_impl::render_target() const
   {
      return _render_target;
   };

   inline d2d_factory* canvas_impl::factory() const
   {
      return _factory;
   }

   inline bool canvas_impl::update()
   {
      if (!_render_target)
      {
         RECT rc;
         GetClientRect(_hwnd, &rc);

         D2D1_SIZE_U size = D2D1::SizeU(
            rc.right - rc.left,
            rc.bottom - rc.top
         );

         // Create a Direct2D render target.
         auto hr = _factory->CreateHwndRenderTarget(
            D2D1::RenderTargetProperties(),
            D2D1::HwndRenderTargetProperties(_hwnd, size),
            &_render_target
         );

         if (SUCCEEDED(hr) && on_update)
            on_update(*this);

         return true;
      }
      return false;
   }

   void canvas_impl::discard()
   {
      release(_render_target);
   }
}

#endif
