/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <artist/image.hpp>

namespace cycfi::artist
{
   image::image(extent size)
   {}

   image::image(fs::path const& path_)
   {
   }

   image::~image()
   {
   }

   image_impl_ptr image::impl() const
   {
      return nullptr;
   }

   extent image::size() const
   {
      return {};
   }

   void image::save_png(std::string_view path_) const
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

