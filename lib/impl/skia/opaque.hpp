/*=============================================================================
   Copyright (c) 2016-2023 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#if !defined(ARTIST_SKIA_OPAQUE_MARCH_22_2020)
#define ARTIST_SKIA_OPAQUE_MARCH_22_2020

#include "SkImage.h"
#include "SkBitmap.h"
#include "SkSurface.h"
#include <artist/point.hpp>
#include <variant>

namespace cycfi::artist
{
   using image_impl_variant = std::variant<extent, sk_sp<SkPicture>, SkBitmap>;

   class image_impl : public image_impl_variant
   {
   public:

      using base_type = image_impl_variant;
      using base_type::base_type;

      base_type&        base() { return *this; }
      base_type const&  base() const { return *this; }

      float             scale = 1.0f;   // bitmap pixels per logical unit

      // The bitmap as the SkImage canvas::draw draws, made once, sharing
      // the bitmap's pixels. asImage copies a mutable bitmap's pixels, and
      // each new SkImage is a new texture to the GPU backends, uploaded
      // (and its mipmaps made) on every draw; one kept image is uploaded
      // once. Dropped when the pixels may have changed: image::pixels() and
      // offscreen_image. While an offscreen_image is drawing into the
      // bitmap, canvas::draw takes a copy each time instead.
      sk_sp<SkImage>    drawn;
      int               offscreens = 0; // offscreen_images drawing into it

      void pixels_changed()
      {
         if (auto* bitmap = std::get_if<SkBitmap>(&base()))
            bitmap->notifyPixelsChanged();
         drawn.reset();
      }
   };

   class canvas_layer_impl
   {
   public:

      sk_sp<SkSurface>  surface;
      extent            size;           // logical units
   };
}

#endif