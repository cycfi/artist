/*=============================================================================
   Copyright (c) 2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <artist/canvas_layer.hpp>
#include "recording_impl.hpp"

namespace cycfi::artist
{
   // The recording backend draws nothing: a layer records into its own
   // journal, and drawing the layer records an image over the destination.
   class canvas_layer_impl
   {
   public:

      recording::journal      journal;
      recording::canvas_impl  ctx;
      extent                  size;
   };

   canvas_layer::canvas_layer(canvas& /*cnv*/, extent size)
    : _impl{new canvas_layer_impl}
   {
      _impl->ctx.out = &_impl->journal;
      _impl->ctx.size = size;
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
