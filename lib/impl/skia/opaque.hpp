/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#if !defined(ARTIST_SKIA_OPAQUE_MARCH_22_2020)
#define ARTIST_SKIA_OPAQUE_MARCH_22_2020

#include "SkImage.h"
#include <variant>

namespace cycfi::artist
{
   class picture_impl : public std::variant<extent, sk_sp<SkPicture>, SkBitmap>
   {
      using base_type = std::variant<extent, sk_sp<SkPicture>, SkBitmap>;
      using base_type::base_type;
   };

   class canvas;
   SkPaint& fill_paint(canvas const& cnv);
}

#endif