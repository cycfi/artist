/*=============================================================================
   Copyright (c) 2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <artist/canvas_layer.hpp>
#include "cairo_private.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace cycfi::artist
{
   // Cairo draws on the CPU, so the layer is an image surface: Cairo's own
   // rasterizer, rather than a surface similar to the target (a Quartz, Win32
   // or Xlib surface draws the same work several times slower). It has the
   // target's device scale and follows the canvas's current transform scale,
   // which some hosts use for the display density.
   canvas_layer::canvas_layer(canvas& cnv, extent size)
    : _impl{new canvas_layer_impl}
   {
      auto* cr = cnv.impl();
      cairo_matrix_t m;
      cairo_get_matrix(cr, &m);
      double const scale = std::max(std::hypot(m.xx, m.yx), 1e-3);

      double dev_x = 1, dev_y = 1;
      cairo_surface_get_device_scale(cairo_get_target(cr), &dev_x, &dev_y);
      int const w = std::max(1, int(std::ceil(size.x * scale * dev_x)));
      int const h = std::max(1, int(std::ceil(size.y * scale * dev_y)));
      _impl->surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
      if (cairo_surface_status(_impl->surface) != CAIRO_STATUS_SUCCESS)
      {
         delete _impl;
         throw std::runtime_error{"artist cairo backend: Failed to create canvas layer."};
      }
      cairo_surface_set_device_scale(_impl->surface, dev_x, dev_y);
      _impl->cr = cairo_create(_impl->surface);
      cairo_scale(_impl->cr, scale, scale);
      _impl->width = size.x;
      _impl->height = size.y;
      _impl->scale = scale;
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
      return _impl->cr;
   }

   extent canvas_layer::size() const
   {
      return {_impl->width, _impl->height};
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
