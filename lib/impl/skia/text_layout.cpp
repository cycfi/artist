/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <string_view>
#include <artist/text_layout.hpp>
#include <artist/canvas.hpp>
#include <vector>

#include <hb.h>
#include <hb-ot.h>
#include <SkStream.h>
#include <SkTypeface.h>

namespace cycfi::artist
{
   namespace
   {
      struct hb_blob
      {
         hb_blob(std::unique_ptr<SkStreamAsset> asset)
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

         hb_blob(hb_blob const&) = delete;

         ~hb_blob()
         {
            hb_blob_destroy(blob);
         }

         hb_blob_t* get() { return blob; }

      private:

         hb_blob_t* blob = nullptr;
      };

      struct hb_font
      {
         hb_font(SkTypeface* tf)
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

         hb_font(hb_font const&) = delete;

         ~hb_font()
         {
            hb_font_destroy(font);
         }

         hb_font_t* get() { return font; }

      private:

         hb_font_t* font = nullptr;
      };
   }

   struct text_layout::impl
   {
      impl(font const& font_, std::string_view utf8)
       : _font{ font_ }
       , _utf8{ utf8 }
      {
      }

      ~impl()
      {
      }

      // void flow(float width, float indent, float line_height, bool justify)
      // {
      // }

      // void  draw(canvas& cnv, point p)
      // {
      // }

      // glyph_position position(char const* text) const
      // {
      //    return {};
      // }

      // char const* hit_test(point p) const
      // {
      //    return nullptr;
      // }

      font              _font;
      std::string_view  _utf8;
   };

   text_layout::text_layout(font const& font_, std::string_view utf8)
    : _impl{ std::make_unique<impl>(font_, utf8) }
   {
   }

   text_layout::~text_layout()
   {
   }

   void text_layout::flow(float width, bool justify)
   {
      // auto line_info_f = [this, width](float y)
      // {
      //    return line_info{ 0, width };
      // };

      // auto lh = _impl->_font.line_height();
      // flow(line_info_f, { justify, lh, lh });
   }

   void text_layout::flow(get_line_info const& glf, flow_info finfo)
   {
   }

   void text_layout::draw(canvas& cnv, point p) const
   {
   }
}

