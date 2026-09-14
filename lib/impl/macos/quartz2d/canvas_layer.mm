/*=============================================================================
   Copyright (c) 2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <artist/canvas_layer.hpp>
#include <Quartz/Quartz.h>
#include "image_impl.hpp"
#include <algorithm>
#include <cmath>

namespace cycfi::artist
{
   // The bitmap has the canvas's device pixels per unit, and its context gets
   // the same flipped (top-left, y-down) transform an offscreen_image gives,
   // once, for the life of the layer.
   canvas_layer::canvas_layer(canvas& cnv, extent size)
    : _impl{nullptr}
   {
      auto ctx = CGContextRef(cnv.impl());
      auto px = CGContextConvertSizeToDeviceSpace(ctx, CGSizeMake(1, 1));
      float const scale = std::max(float(std::abs(px.width)), 1e-3f);

      _impl = new canvas_layer_impl{size, scale};
      auto* ip = _impl->img.impl();
      CGContextTranslateCTM(ip->ctx(), 0, ip->height());
      CGContextScaleCTM(ip->ctx(), scale, -scale);
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
      return (canvas_impl*) _impl->img.impl()->ctx();
   }

   extent canvas_layer::size() const
   {
      return _impl->img.size();
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
