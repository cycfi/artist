/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <string_view>
#include <string>
#include <artist/text_layout.hpp>
#include <artist/canvas.hpp>
#include <vector>
#include <SkFont.h>
#include "detail/harfbuzz.hpp"
#include "linebreak.h"
#include "unicode.hpp"

#include <iostream>

namespace cycfi::artist
{
   struct text_layout::impl
   {
      impl(font const& font_, std::string_view utf8)
       : _font{ font_ }
       , _hb_font(_font.impl()->getTypeface())
       , _utf8{ utf8 }
       , _buff{ utf8 }
      {
         // $$$ For now... later, this should be extracted from the text, perhaps?
         _buff.direction(HB_DIRECTION_LTR);
         _buff.script(HB_SCRIPT_LATIN);
         _buff.language("en");
      }

      ~impl()
      {
      }

      void flow(get_line_info const& glf, flow_info finfo)
      {
         _buff.shape(_hb_font);
         auto glyphs_info = _buff.glyphs();

         int hb_scalex, hb_scaley;
         hb_font_get_scale(_hb_font.get(), &hb_scalex, &hb_scaley);
         auto sc_font = _font.impl();
         float scalex = (sc_font->getSize() / hb_scalex) * sc_font->getScaleX();

         std::vector<float> positions;
         std::vector<char32_t> line;
         positions.reserve(glyphs_info.count);
         auto linfo = glf(0);
         float y = 0;
         float x = linfo.offset;
         auto start = 0;

         // $$$ JDG "en" for now $$$
         std::string brks(_utf8.size(), 0);
         set_linebreaks_utf8((utf8_t const*)_utf8.begin(), _utf8.size(), "en", brks.data());

         auto new_line = [&](std::size_t start_line, std::size_t idx, std::size_t& i)
            {
               auto utf8_index = idx;
               auto brk_index = _buff.glyph_index(utf8_index);
               auto brk_cluster = glyphs_info.glyphs[brk_index].cluster;

               positions.clear();
               i = brk_index;
               start = brk_index + 1;
               y += finfo.line_height;
               linfo = glf(y);
               x = linfo.offset;

               std::cout << std::string_view(_utf8.begin() + start_line, utf8_index - start_line) << std::endl;
            };

         for (std::size_t i = 0; i != glyphs_info.count; ++i)
         {
            positions.push_back(x + (glyphs_info.positions[i].x_offset * scalex));
            x += glyphs_info.positions[i].x_advance * scalex;

            if (auto idx = glyphs_info.glyphs[i].cluster; brks[idx] == LINEBREAK_MUSTBREAK)
            {
               // We must break now
               auto start_line = glyphs_info.glyphs[start].cluster;
               new_line(start_line, idx, i);
            }
            else if (x > linfo.width)
            {
               // We break the line when x exceeds the target width
               std::size_t len = positions.size();
               auto start_line = glyphs_info.glyphs[start].cluster;
               auto end_line = glyphs_info.glyphs[start+len].cluster;
               auto brks_len = end_line-start_line;
               auto brks_line = brks.substr(start_line, brks_len);
               auto pos = brks_line.find_last_of(char(LINEBREAK_ALLOWBREAK), brks_len-1);

               if (pos != brks_line.npos)
                  new_line(start_line, pos + start_line, i);
            }
         }
      }

      font                    _font;
      detail::hb_font         _hb_font;
      std::string_view        _utf8;
      detail::hb_buffer       _buff;
   };

   void foo() {}

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

