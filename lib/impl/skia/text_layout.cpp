/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <string_view>
#include <artist/text_layout.hpp>
#include <artist/canvas.hpp>
#include <vector>
#include <SkFont.h>
#include "detail/harfbuzz.hpp"
#include "linebreak.h"

namespace cycfi::artist
{
   struct text_layout::impl
   {
      impl(font const& font_, std::string_view utf8)
       : _font{ font_ }
       , _hb_font(_font.impl()->getTypeface())
       , _utf8{ utf8 }
       , _buff(hb_buffer_create())
      //  , _locale("en") // $$$ For now
      {
         hb_buffer_add_utf8(_buff, utf8.data(), utf8.size(), 0, utf8.size());

         // $$$ For now... later, this should be extracted from the text, perhaps?
         hb_buffer_set_direction(_buff, HB_DIRECTION_LTR);
         hb_buffer_set_script(_buff, HB_SCRIPT_LATIN);
         hb_buffer_set_language(_buff, hb_language_from_string("en", -1));
      }

      ~impl()
      {
         hb_buffer_destroy(_buff);
      }

      void flow(get_line_info const& glf, flow_info finfo)
      {
         unsigned int glyph_count = 0;
         hb_glyph_info_t* glyph_info = hb_buffer_get_glyph_infos(_buff, &glyph_count);
         hb_glyph_position_t* glyph_pos = hb_buffer_get_glyph_positions(_buff, &glyph_count);


      }

      font              _font;
      detail::hb_font   _hb_font;
      std::string_view  _utf8;
      hb_buffer_t*      _buff;
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
      auto line_info_f = [this, width](float y)
      {
         return line_info{ 0, width };
      };

      auto lh = _impl->_font.line_height();
      flow(line_info_f, { justify, lh, lh });
   }

   void text_layout::flow(get_line_info const& glf, flow_info finfo)
   {
      _impl->flow(glf, finfo);
   }

   void text_layout::draw(canvas& cnv, point p) const
   {
   }
}

