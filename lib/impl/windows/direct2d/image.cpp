/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]

   BASELINE STUB. Real WIC implementation lands in task #6 (image load/create,
   save_png, pixels/bitmap_size, and offscreen_image -> a WIC bitmap render
   target wrapped in a d2d::context for headless rendering).
=============================================================================*/
#include <artist/image.hpp>
#include "context.hpp"

namespace cycfi::artist
{
   image::image(extent /*size*/)
    : _impl(nullptr)
   {
   }

   image::image(fs::path const& /*path_*/)
    : _impl(nullptr)
   {
   }

   image::image(uint8_t const* /*data*/, pixel_format /*fmt*/, extent /*size*/)
    : _impl(nullptr)
   {
   }

   image::~image()
   {
   }

   image_impl_ptr image::impl() const
   {
      return _impl;
   }

   extent image::size() const
   {
      return {};
   }

   void image::save_png(std::string_view /*path_*/) const
   {
   }

   uint32_t* image::pixels()
   {
      return nullptr;
   }

   uint32_t const* image::pixels() const
   {
      return nullptr;
   }

   extent image::bitmap_size() const
   {
      return {};
   }

   offscreen_image::offscreen_image(image& img)
    : _image(img)
   {
   }

   offscreen_image::~offscreen_image()
   {
   }

   canvas_impl* offscreen_image::context() const
   {
      return nullptr;
   }
}
