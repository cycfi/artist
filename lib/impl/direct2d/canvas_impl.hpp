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
#include <memory>
#include <stdexcept>

namespace cycfi::artist
{
   template <class Interface>
   inline void release(Interface*& ptr)
   {
      if (ptr)
      {
         ptr->Release();
         ptr = nullptr;
      }
   }

   using d2d_canvas = ID2D1HwndRenderTarget;
   using d2d_factory = ID2D1Factory;

   // using d2d_color = ID2D1SolidColorBrush;
   // using d2d_linear_gradient = ID2D1LinearGradientBrush;
   // using d2d_radial_gradient = ID2D1RadialGradientBrush;

   // using fill_brush = std::variant<
   //    std::pair<color, d2d_color*>
   //  , std::pair<canvas::linear_gradient, d2d_linear_gradient*>
   //  , std::pair<canvas::radial_gradient, d2d_radial_gradient>
   // >;

   d2d_factory& get_factory();

   struct canvas_state_impl
   {
      virtual void         update(d2d_canvas& cnv) = 0;
      virtual void         discard() = 0;
   };

   struct canvas_impl
   {
   public:
                           canvas_impl(HWND hwnd, color bkd);
                           ~canvas_impl();

                           template <typename Renderer>
      void                 render(Renderer&& draw);

      HWND                 hwnd() const;
      d2d_canvas*          canvas() const;

      void                 state(canvas_state_impl* state);
      canvas_state_impl*   state() const;

   private:

                           canvas_impl(canvas_impl const&) = delete;
      canvas_impl&         operator=(canvas_impl const&) = delete;

      void                 update();
      void                 discard();

      HWND                 _hwnd = nullptr;
      d2d_canvas*          _d2d_canvas = nullptr;
      D2D1::ColorF         _bkd;
      canvas_state_impl*   _state = nullptr;
   };

   ////////////////////////////////////////////////////////////////////////////
   // Inlines
   ////////////////////////////////////////////////////////////////////////////
   namespace detail
   {
      struct release_factory
      {
         void operator()(d2d_factory* ptr)
         {
            release(ptr);
         }
      };
   }

   inline d2d_factory& get_factory()
   {
      using unique_ptr = std::unique_ptr<d2d_factory, detail::release_factory>;
      static d2d_factory* ptr = nullptr;
      D2D1CreateFactory(D2D1_FACTORY_TYPE_MULTI_THREADED, &ptr);
      static auto factory_ptr = unique_ptr(ptr);
      return *ptr;
   }

   inline canvas_impl::canvas_impl(HWND hwnd, color bkd)
    : _hwnd{ hwnd }
    , _bkd{ bkd.red, bkd.green, bkd.blue, bkd.alpha }
   {
   }

   inline canvas_impl::~canvas_impl()
   {
      release(_d2d_canvas);
   }

   inline HWND canvas_impl::hwnd() const
   {
      return _hwnd;
   }

   inline d2d_canvas* canvas_impl::canvas() const
   {
      return _d2d_canvas;
   };

   template <typename Renderer>
   void canvas_impl::render(Renderer&& draw)
   {
      update();

      if (!(_d2d_canvas->CheckWindowState() & D2D1_WINDOW_STATE_OCCLUDED))
      {
         _d2d_canvas->BeginDraw();
         draw(*_d2d_canvas);
         auto hr = _d2d_canvas->EndDraw();
         if (hr == D2DERR_RECREATE_TARGET)
            discard();
      }
   }

   inline void canvas_impl::update()
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

   inline void canvas_impl::discard()
   {
      if (_state)
         _state->discard();
      release(_d2d_canvas);
   }

   inline void canvas_impl::state(canvas_state_impl* state)
   {
      _state = state;
   }

   inline canvas_state_impl* canvas_impl::state() const
   {
      return _state;
   }
}

#endif
