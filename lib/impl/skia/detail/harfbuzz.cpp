/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include "harfbuzz.hpp"

namespace cycfi::artist::detail
{
   hb_blob::hb_blob(std::unique_ptr<SkStreamAsset> asset)
   {
      std::size_t size = asset->getLength();
      hb_blob_t* blob;
      if (const void* base = asset->getMemoryBase())
      {
         blob = hb_blob_create(
            (char*)base
            , SkToUInt(size)
            , HB_MEMORY_MODE_READONLY
            , asset.release()
            , [](void* p) { delete (SkStreamAsset*)p; }
         );
      }
      else
      {
         void* ptr = size ? std::malloc(size) : nullptr;
         asset->read(ptr, size);
         blob = hb_blob_create(
            (char*)ptr
            , SkToUInt(size)
            , HB_MEMORY_MODE_READONLY
            , ptr
            , std::free
         );
      }
      SkASSERT(blob);
      hb_blob_make_immutable(blob);
   }

   hb_font::hb_font(SkTypeface* tf)
   {
      int index;
      hb_blob blob{ std::unique_ptr<SkStreamAsset>(tf->openStream(&index)) };
      hb_face_t* face = hb_face_create(blob.get(), unsigned(index));
      SkASSERT(face);
      if (face)
      {
         hb_face_set_index(face, unsigned(index));
         hb_face_set_upem(face, tf->getUnitsPerEm());

         font = hb_font_create(face);
         SkASSERT(font);
         if (font)
         {
            hb_ot_font_set_funcs(font);
         }
         hb_face_destroy(face);
         int axis_count = tf->getVariationDesignPosition(nullptr, 0);

         if (axis_count > 0)
         {
            SkAutoSTMalloc<4, SkFontArguments::VariationPosition::Coordinate> axis_values(axis_count);
            if (tf->getVariationDesignPosition(axis_values, axis_count) == axis_count)
            {
               hb_font_set_variations(
                  font
                  , reinterpret_cast<hb_variation_t*>(axis_values.get())
                  , axis_count
               );
            }
         }
      }
   }
}

