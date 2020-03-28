/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include "harfbuzz.hpp"
#include <hb.h>
#include <hb-ot.h>
#include <SkStream.h>
#include <SkTypeface.h>

namespace cycfi::artist::detail
{
   hb_blob::hb_blob(std::unique_ptr<SkStreamAsset> asset)
   {
      std::size_t size = asset->getLength();
      if (const void* base = asset->getMemoryBase())
      {
         _blob = hb_blob_create(
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
         _blob = hb_blob_create(
            (char*)ptr
           , SkToUInt(size)
           , HB_MEMORY_MODE_READONLY
           , ptr
           , std::free
         );
      }
      SkASSERT(_blob);
      hb_blob_make_immutable(_blob);
   }

   hb_blob::~hb_blob()
   {
      hb_blob_destroy(_blob);
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

         _font = hb_font_create(face);
         SkASSERT(_font);
         if (_font)
         {
            hb_ot_font_set_funcs(_font);
            int axis_count = tf->getVariationDesignPosition(nullptr, 0);
            if (axis_count > 0)
            {
               SkAutoSTMalloc<4, SkFontArguments::VariationPosition::Coordinate> axis_values(axis_count);
               if (tf->getVariationDesignPosition(axis_values, axis_count) == axis_count)
               {
                  hb_font_set_variations(
                     _font
                  , reinterpret_cast<hb_variation_t*>(axis_values.get())
                  , axis_count
                  );
               }
            }
         }
         else
         {
            _font = nullptr;
         }
         hb_face_destroy(face);
      }
   }

   hb_font::~hb_font()
   {
      hb_font_destroy(_font);
   }


   void foo() {}

   hb_buffer::hb_buffer(std::string_view utf8)
    : _buffer(hb_buffer_create())
   {
      hb_buffer_add_utf8(_buffer, utf8.data(), utf8.size(), 0, utf8.size());
      _map.insert(_map.begin(), hb_buffer_get_length(_buffer), -1);
   }

   hb_buffer::~hb_buffer()
   {
      hb_buffer_destroy(_buffer);
   }

   void hb_buffer::direction(hb_direction_t dir)
   {
      hb_buffer_set_direction(_buffer, dir);
   }

   void hb_buffer::script(hb_script_t scr)
   {
      hb_buffer_set_script(_buffer, scr);
   }

   void hb_buffer::language(std::string_view lang)
   {
      hb_buffer_set_language(_buffer, hb_language_from_string(lang.data(), lang.size()));
   }

   void hb_buffer::shape(hb_font const& font)
   {
      hb_shape(font.get(), _buffer, nullptr, 0);

      unsigned int count;
      auto glyphs = hb_buffer_get_glyph_infos(_buffer, &count);

      for (auto i = 0; i != count; ++i)
         _map[glyphs[i].cluster] = i;
   }

   hb_buffer::glyphs_info hb_buffer::glyphs() const
   {
      glyphs_info info;
      info.glyphs = hb_buffer_get_glyph_infos(_buffer, &info.count);
      info.positions = hb_buffer_get_glyph_positions(_buffer, &info.count);
      return info;
   }

   int hb_buffer::glyph_index(std::size_t index) const
   {
      auto i = _map[index];
      while (i == -1 && index > 0)
      {
         --index;
         i = _map[index];
      }
      return i;
   }
}

