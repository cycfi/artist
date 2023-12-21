/*=============================================================================
   Copyright (c) 2016-2023 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#if !defined(ARTIST_SKIA_OPAQUE_MARCH_22_2020)
#define ARTIST_SKIA_OPAQUE_MARCH_22_2020

#include "SkImage.h"
#include "SkBitmap.h"
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
   };
}

#endif