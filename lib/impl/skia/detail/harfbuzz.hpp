/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <infra/support.hpp>
#include <memory>
#include <string_view>
#include <vector>
#include <hb.h>
#include <hb-ot.h>

class SkStreamAsset;
class SkTypeface;

namespace cycfi::artist::detail
{
   class hb_blob : non_copyable
   {
   public:
                              hb_blob(std::unique_ptr<SkStreamAsset> asset);
      hb_blob_t*              get() const { return _blob.get(); }

   private:

      using ptr_type = std::unique_ptr<hb_blob_t, deleter<hb_blob_t, hb_blob_destroy>>;

      ptr_type                _blob;
   };

   struct hb_font
   {
   public:
                              hb_font(SkTypeface* tf);
      hb_font_t*              get() const { return _font.get(); }

   private:

      using ptr_type = std::unique_ptr<hb_font_t, deleter<hb_font_t, hb_font_destroy>>;

      ptr_type                _font;
   };

   class hb_buffer : non_copyable
   {
   public:
                              hb_buffer(std::string_view utf8);

      void                    direction(hb_direction_t dir);
      void                    script(hb_script_t scr);
      void                    language(char const* lang);
      char const*             language() const;

      struct glyphs_info
      {
         unsigned int         count;
         hb_glyph_info_t*     glyphs;
         hb_glyph_position_t* positions;
      };

      void                    shape(hb_font const& font);
      glyphs_info             glyphs() const;

      hb_buffer_t*            get() const { return _buffer.get(); }
      int                     glyph_index(std::size_t index) const;

   private:

      using ptr_type = std::unique_ptr<hb_buffer_t, deleter<hb_buffer_t, hb_buffer_destroy>>;

      ptr_type             _buffer;
      std::vector<int>     _map;
   };
}

