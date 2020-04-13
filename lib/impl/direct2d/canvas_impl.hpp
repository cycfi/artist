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
   using d2d_canvas = ID2D1HwndRenderTarget;
   using d2d_factory = ID2D1Factory;

   ////////////////////////////////////////////////////////////////////////////
   // The main factory (singleton)
   ////////////////////////////////////////////////////////////////////////////
   d2d_factory& get_factory();

   ////////////////////////////////////////////////////////////////////////////
   // Abstract canvas state implementation
   ////////////////////////////////////////////////////////////////////////////
   struct canvas_state_impl
   {
      virtual void         update(d2d_canvas& cnv) = 0;
      virtual void         discard() = 0;
   };

   ////////////////////////////////////////////////////////////////////////////
   // Main (low-level) canvas implementation
   ////////////////////////////////////////////////////////////////////////////
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
   // Low-level D2D types
   ////////////////////////////////////////////////////////////////////////////
   using d2d_paint = ID2D1SolidColorBrush;
   using d2d_geometry = ID2D1Geometry;
   using d2d_geometry_group = ID2D1GeometryGroup;
   using d2d_fill_mode = D2D1_FILL_MODE;

   ////////////////////////////////////////////////////////////////////////////
   // Low-level utilities
   ////////////////////////////////////////////////////////////////////////////
   template <typename Interface>
   inline void release(Interface*& ptr);

   d2d_paint* make_brush(color c);

   template <typename Container>
   d2d_geometry_group* make_group(Container const& c);

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

   inline d2d_paint* make_paint(color c, d2d_canvas& cnv)
   {
      d2d_paint* ptr = nullptr;
      auto hr = cnv.CreateSolidColorBrush(
         D2D1::ColorF(c.red, c.green, c.blue, c.alpha)
       , &ptr
      );
      if (!SUCCEEDED(hr))
         throw std::runtime_error{ "Error: CreateSolidColorBrush Fail." };
      return ptr;
   }

   template <typename Container>
   inline d2d_geometry_group* make_group(
      Container const& c, d2d_fill_mode mode
   )
   {
      d2d_geometry_group* group = nullptr;
      auto hr = get_factory().CreateGeometryGroup(
         mode
       , const_cast<d2d_geometry**>(c.data())
       , c.size()
       , &group
      );
      if (!SUCCEEDED(hr))
         throw std::runtime_error{ "Error: CreateGeometryGroup Fail." };
      return group;
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
