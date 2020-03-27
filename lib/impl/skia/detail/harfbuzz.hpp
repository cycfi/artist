/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <memory>
#include <string_view>
#include <hb.h>
#include <hb-ot.h>

struct SkStreamAsset;
struct SkTypeface;

namespace cycfi::artist::detail
{
   class hb_blob
   {
   public:
                           hb_blob(std::unique_ptr<SkStreamAsset> asset);
                           hb_blob(hb_blob const&) = delete;
                           ~hb_blob();

      hb_blob_t*           get() const { return _blob; }

   private:

      hb_blob_t* _blob = nullptr;
   };

   struct hb_font
   {
   public:
                           hb_font(SkTypeface* tf);
                           hb_font(hb_font const&) = delete;
                           ~hb_font();

      hb_font_t*           get() const { return _font; }

   private:

      hb_font_t* _font = nullptr;
   };

   class hb_buffer
   {
   public:
                           hb_buffer(std::string_view utf8);
                           hb_buffer(hb_buffer const&) = delete;
                           ~hb_buffer();

      void                 direction(hb_direction_t dir);
      void                 script(hb_script_t scr);
      void                 language(std::string_view lang);

      struct glyphs_info
      {
         unsigned int         count;
         hb_glyph_info_t*     glyphs;
         hb_glyph_position_t* positions;
      };

      void                 shape(hb_font const& font);
      glyphs_info          glyphs() const;

      hb_buffer_t*         get() const { return _buffer; }

   private:

      hb_buffer_t*         _buffer = nullptr;
   };
}

