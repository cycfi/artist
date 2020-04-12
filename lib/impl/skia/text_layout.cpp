/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <string_view>
#include <string>
#include <artist/text_layout.hpp>
#include <artist/canvas.hpp>
#include <artist/unicode.hpp>
#include <vector>
#include <SkFont.h>
#include <SkTextBlob.h>
#include <SkCanvas.h>
#include "detail/harfbuzz.hpp"
#include "linebreak.h"
#include "detail/image_impl.hpp"

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

         init_linebreak();
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

         std::vector<SkScalar> positions;
         positions.reserve(glyphs_info.count);
         auto linfo = glf(0);
         float y = 0;
         float x = 0;
         auto glyph_start = 0;

         std::string brks(_utf8.size(), 0);
         set_linebreaks_utf8(
            (utf8_t const*)_utf8.data()
          , _utf8.size(), _buff.language(), brks.data()
         );

         auto num_spaces =
            [&](std::size_t glyph_idx) -> std::size_t
            {
               std::size_t count = 0;
               for (auto i = glyph_start; i != glyph_idx; ++i)
               {
                  auto cl = glyphs_info.glyphs[i].cluster;
                  if (unicode::is_space(&_utf8[cl]))
                     ++count;
               }
               return count;
            };

         auto justify =
            [&](std::size_t glyph_idx, bool must_break)
            {
               if (finfo.justify && !must_break)
               {
                  auto line_width =
                     positions[glyph_idx-glyph_start] +
                     (glyphs_info.positions[glyph_idx].x_advance * scalex)
                  ;
                  if (((line_width / linfo.width) > 0.9))
                  {
                     // Full justify
                     auto nspaces = num_spaces(glyph_idx);
                     float extra = (linfo.width - line_width) / nspaces;
                     float offset = 0;
                     for (auto i = glyph_start; i != glyph_idx; ++i)
                     {
                        using namespace unicode;
                        auto cl = glyphs_info.glyphs[i].cluster;
                        if (unicode::is_space(&_utf8[cl]))
                           offset += extra;
                        positions[i-glyph_start] += offset;
                     }
                  }
               }
            };

         auto new_line =
            [&](std::size_t utf8_idx, std::size_t& i, bool must_break)
            {
               auto glyph_idx = _buff.glyph_index(utf8_idx);
               justify(glyph_idx, must_break);

               auto glyph_count = glyph_idx - glyph_start;
               if (i == glyphs_info.count-1)
                  ++glyph_count;

               std::vector<SkGlyphID> line_glyphs(glyph_count);
               for (auto j = 0; j != glyph_count; ++j)
                  line_glyphs[j] = glyphs_info.glyphs[glyph_start + j].codepoint;

               _rows.push_back(
                  std::make_pair(
                     point{ linfo.offset, y }
                   , SkTextBlob::MakeFromPosTextH(
                        line_glyphs.data(), line_glyphs.size() * sizeof(SkGlyphID)
                     , positions.data(), 0
                     , *_font.impl()
                     , SkTextEncoding::kGlyphID)
                  )
               );

               positions.clear();
               i = glyph_idx;
               glyph_start = glyph_idx + 1;
               y += finfo.line_height;
               linfo = glf(y);
               x = 0;
            };

         for (std::size_t i = 0; i != glyphs_info.count; ++i)
         {
            positions.push_back(x + (glyphs_info.positions[i].x_offset * scalex));
            x += glyphs_info.positions[i].x_advance * scalex;

            if (auto idx = glyphs_info.glyphs[i].cluster; brks[idx] == LINEBREAK_MUSTBREAK)
            {
               // We must break now
               new_line(idx, i, true);
            }
            else if (x > linfo.width)
            {
               // We break the line when x exceeds the target width
               std::size_t len = positions.size();
               auto start_line = glyphs_info.glyphs[glyph_start].cluster;
               auto end_line = glyphs_info.glyphs[glyph_start+len].cluster;
               auto brks_len = end_line-start_line;
               auto brks_line = brks.substr(start_line, brks_len);
               auto pos = brks_line.find_last_of(char(LINEBREAK_ALLOWBREAK), brks_len-1);

               if (pos != brks_line.npos)
                  new_line(pos + start_line, i, false);
               else
                  ; // deal with the case where we have to forcefully break the line
            }
         }
      }

      void  draw(canvas& cnv, point p)
      {
         for (auto const& line : _rows)
         {
            auto sk_cnv = cnv.impl();
            sk_cnv->drawTextBlob(
               line.second
             , p.x+line.first.x, p.y+line.first.y
             , fill_paint(cnv)
            );
         }
      }

      using line_vector = std::vector<std::pair<point, sk_sp<SkTextBlob>>>;

      font                    _font;
      detail::hb_font         _hb_font;
      std::string_view        _utf8;
      detail::hb_buffer       _buff;
      line_vector             _rows;
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
      auto line_info_f = [this, width](float /*y*/)
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
      _impl->draw(cnv, p);
   }
}

