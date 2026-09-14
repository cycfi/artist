/*=============================================================================
   Copyright (c) 2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <artist/canvas_layer.hpp>

#include "SkCanvas.h"
#include "SkMatrix.h"
#include "SkSurface.h"

#include "opaque.hpp"
#include <algorithm>
#include <stdexcept>

namespace cycfi::artist
{
   // The surface comes from the canvas itself (SkCanvas::makeSurface), so a
   // GPU canvas gets a GPU surface on the same context and a raster canvas a
   // raster one. Its pixels follow the canvas's current scale, which the hosts
   // set to the display's pixel density.
   canvas_layer::canvas_layer(canvas& cnv, extent size)
    : _impl{new canvas_layer_impl}
   {
      auto* sk = cnv.impl();
      SkScalar scales[2] = {1, 1};
      if (!sk->getTotalMatrix().getMinMaxScales(scales))
         scales[1] = 1;
      float const scale = std::max(scales[1], SkScalar(1e-3));

      int const w = std::max(1, int(size.x * scale + 0.5f));
      int const h = std::max(1, int(size.y * scale + 0.5f));
      auto target = sk->imageInfo();
      auto info = target.colorType() == kUnknown_SkColorType?
         SkImageInfo::MakeN32Premul(w, h) :
         target.makeWH(w, h).makeAlphaType(kPremul_SkAlphaType);

      _impl->surface = sk->makeSurface(info);
      if (!_impl->surface)
         _impl->surface = SkSurfaces::Raster(info);
      if (!_impl->surface)
      {
         delete _impl;
         throw std::runtime_error{"artist skia backend: Failed to create canvas layer."};
      }
      _impl->surface->getCanvas()->scale(scale, scale);
      _impl->size = size;
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
      return _impl->surface->getCanvas();
   }

   extent canvas_layer::size() const
   {
      return _impl->size;
   }

   canvas_layer_impl* canvas_layer::impl() const
   {
      return _impl;
   }
}
