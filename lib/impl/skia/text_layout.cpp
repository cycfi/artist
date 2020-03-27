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
         positions.reserve(glyphs_info.count);
         auto linfo = glf(0);
         float y = 0;
         float x = linfo.offset;
         auto start = 0;

         for (auto i = 0; i != glyphs_info.count; ++i)
         {
            positions.push_back(x + (glyphs_info.positions[i].x_offset * scalex));
            x += glyphs_info.positions[i].x_advance * scalex;
            if (x > linfo.width)
            {
               // here we break the line
               std::size_t len = positions.size();
               if (len > 2)
               {
                  auto r_cluster = glyphs_info.glyphs[start].cluster;
                  auto rng = _utf8.substr(r_cluster, len);

                  // $$$ JDG "en" for now $$$
                  std::string brks(len, 0);
                  set_linebreaks_utf8((utf8_t const*)rng.begin(), len, "en", brks.data());

                  constexpr char const breaks[2] = { LINEBREAK_ALLOWBREAK, LINEBREAK_MUSTBREAK };
                  auto pos = brks.find_last_of(breaks, len-2, 2);
                  if (pos != brks.npos)
                  {
                     auto brk_index = pos;
                     auto brk_cluster = glyphs_info.glyphs[brk_index].cluster;
                     foo();

                     positions.erase(positions.begin() + brk_index, positions.end());
                     start = brk_index;
                     y += finfo.line_height;
                     linfo = glf(y);
                     x = linfo.offset;
                     continue;
                  }
               }
               else
               {
                  // $$$ deal with this $$$
               }
            }
         }
      }

      void foo() {}

      font              _font;
      detail::hb_font   _hb_font;
      std::string_view  _utf8;
      detail::hb_buffer _buff;
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

      auto lh = _impl->_font.impl()->getSize();
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

