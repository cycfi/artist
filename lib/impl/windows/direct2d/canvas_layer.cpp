/*=============================================================================
   Copyright (c) 2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <artist/canvas_layer.hpp>
#include "context.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace cycfi::artist
{
   canvas_layer::canvas_layer(canvas& cnv, extent size)
    : _impl{new canvas_layer_impl}
   {
      // The layer's pixels follow the target's density and the scale in
      // force on the canvas, so a layer made under scale(2, 2) holds the
      // detail it will be shown at, as on the other backends.
      auto* target = cnv.impl()->target();
      D2D1_SIZE_U pixels = {};
      if (target)
      {
         // The canvas holds its transform until it draws, so the scale comes
         // from the canvas, not the target.
         auto m = cnv.transform();
         float dpi_x, dpi_y;
         target->GetDpi(&dpi_x, &dpi_y);
         auto sx = float(std::hypot(m.a, m.b)) * dpi_x / 96;
         auto sy = float(std::hypot(m.c, m.d)) * dpi_y / 96;
         pixels = D2D1::SizeU(
            UINT32(std::max(1.0f, std::ceil(size.x * sx)))
          , UINT32(std::max(1.0f, std::ceil(size.y * sy)))
         );
      }
      if (!target || !SUCCEEDED(target->CreateCompatibleRenderTarget(
            D2D1::SizeF(size.x, size.y), pixels, &_impl->rt)))
      {
         delete _impl;
         throw std::runtime_error{"artist direct2d backend: Failed to create canvas layer."};
      }
      _impl->ctx.target(_impl->rt);
      _impl->size = size;
      _impl->rt->BeginDraw();
      _impl->rt->Clear(D2D1::ColorF(0, 0, 0, 0));
      _impl->drawing = true;
   }

   canvas_layer::canvas_layer(canvas_layer&& rhs) noexcept
    : _impl{rhs._impl}
   {
      rhs._impl = nullptr;
   }

   canvas_layer::~canvas_layer()
   {
      delete _impl;
   }

   canvas_layer& canvas_layer::operator=(canvas_layer&& rhs) noexcept
   {
      if (this != &rhs)
      {
         delete _impl;
         _impl = rhs._impl;
         rhs._impl = nullptr;
      }
      return *this;
   }

   canvas_impl* canvas_layer::context() const
   {
      return &_impl->ctx;
   }

   extent canvas_layer::size() const
   {
      return _impl->size;
   }

   canvas_layer_impl* canvas_layer::impl() const
   {
      return _impl;
   }

   void canvas::draw(canvas_layer const& layer, point pos)
   {
      auto const size = layer.size();
      draw(layer, rect{pos.x, pos.y, pos.x + size.x, pos.y + size.y});
   }
}
