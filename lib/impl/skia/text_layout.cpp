/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <string_view>
#include <string>
#include <artist/text_layout.hpp>
#include <artist/canvas.hpp>
#include <infra/utf8_utils.hpp>
#include <vector>
#include <SkFont.h>
#include <SkTextBlob.h>
#include <SkCanvas.h>
#include "detail/harfbuzz.hpp"
#include "linebreak.h"
#include "wordbreak.h"

namespace cycfi::artist
{
   class text_layout::impl : non_copyable
   {
   public:

      impl(font const& font_, std::u32string_view utf32);
      ~impl();

      struct row_info
      {
         point                   pos;
         float                   width;
         float                   height;
         std::size_t             glyph_count;
         std::size_t             glyph_index;
         sk_sp<SkTextBlob>       line;
         std::vector<SkScalar>   positions;
      };

      using break_enum = text_layout::break_enum;

      struct break_info
      {
         break_enum              line : 4;
         break_enum              word : 4;
      };

      void                       flow(get_line_info const& glf, flow_info finfo);
      void                       draw(canvas& cnv, point p, color c);
      point                      caret_point(std::size_t index) const;
      std::size_t                caret_index(point p) const;

      std::size_t                num_lines() const;
      font&                      get_font();
      std::u32string_view        get_text() const;

      break_enum                 line_break(std::size_t index) const;
      break_enum                 word_break(std::size_t index) const;

   private:

      using line_vector = std::vector<row_info>;

      class font                 _font;
      detail::hb_font            _hb_font;
      std::u32string             _text;
      detail::hb_buffer          _buff;
      line_vector                _rows;
      SkPaint                    _paint;
      std::vector<break_info>    _breaks;
   };

   text_layout::impl::impl(font const& font_, std::u32string_view utf32)
    : _font{ font_ }
    , _hb_font(_font.impl()->getTypeface())
    , _text{ utf32 }
    , _buff{ _text }
    , _breaks{ utf32.size(), break_info{} }
   {
      struct init_linebreak_
      {
         init_linebreak_()
         {
            init_linebreak();
            init_wordbreak();
         }
      };
      static init_linebreak_ init;

      _paint.setAntiAlias(true);
      _paint.setStyle(SkPaint::kFill_Style);

      _buff.shape(_hb_font);

      std::string lbrks(_text.size(), 0);
      set_linebreaks_utf32(
         (utf32_t const*)_text.data()
         , _text.size(), _buff.language(), lbrks.data()
      );

      std::string wbrks(_text.size(), 0);
      set_wordbreaks_utf32(
         (utf32_t const*)_text.data()
         , _text.size(), _buff.language(), wbrks.data()
      );

      for (std::size_t i = 0; i != _breaks.size(); ++i)
      {
         auto info = break_info{};
         switch (lbrks[i])
         {
            case LINEBREAK_MUSTBREAK:  info.line = must_break;    break;
            case LINEBREAK_ALLOWBREAK: info.line = allow_break;   break;
            case LINEBREAK_NOBREAK:    info.line = no_break;      break;
            default:                   info.line = indeterminate; break;
         }
         switch (wbrks[i])
         {
            case WORDBREAK_BREAK:      info.word = allow_break;   break;
            case WORDBREAK_NOBREAK:    info.word = no_break;      break;
            default:                   info.word = indeterminate; break;
         }
         _breaks[i] = info;
      }
   }

   text_layout::impl::~impl()
   {
   }

   void text_layout::impl::flow(get_line_info const& glf, flow_info finfo)
   {
      if (_text.size() == 0)
         return;
      _rows.clear();

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
      std::size_t glyph_start = 0;

      auto num_spaces =
         [&](std::size_t glyph_idx) -> std::size_t
         {
            std::size_t count = 0;
            for (auto i = glyph_start; i != glyph_idx; ++i)
            {
               auto cl = glyphs_info.glyphs[i].cluster;
               if (is_space(_text[cl]))
                  ++count;
            }
            return count;
         };

      auto justify =
         [&](std::size_t glyph_idx, bool must_break) -> float
         {
             while (glyph_idx >= glyph_start && glyphs_info.glyphs[glyph_idx].codepoint == 0)
                --glyph_idx;

            auto line_width =
               positions[glyph_idx-glyph_start] +
               (glyphs_info.positions[glyph_idx].x_advance * scalex)
            ;
            if (finfo.justify && !must_break)
            {
               if (((line_width / linfo.width) > 0.9))
               {
                  // Full justify
                  auto nspaces = num_spaces(glyph_idx);
                  float extra = (linfo.width - line_width) / nspaces;
                  float offset = 0;
                  for (auto i = glyph_start; i != glyph_idx; ++i)
                  {
                     auto cl = glyphs_info.glyphs[i].cluster;
                     if (is_space(_text[cl]))
                        offset += extra;
                     positions[i-glyph_start] += offset;
                  }
                  if (glyph_idx < positions.size())
                     positions[glyph_idx] += offset;
                  line_width = linfo.width;
               }
            }
            return line_width;
         };

      auto new_line =
         [&](std::size_t text_idx, std::size_t& glyph_idx, bool must_break, bool indeterminate)
         {
            glyph_idx = glyphs_info.glyph_index(text_idx);
            auto glyph_count = glyph_idx - glyph_start;
            if (indeterminate)  // Is the last glyph indeterminate?
               ++glyph_count;

            auto line_width = glyph_count? justify(glyph_idx, must_break) : 0;

            std::vector<SkGlyphID> line_glyphs(glyph_count);
            for (std::size_t j = 0; j != glyph_count; ++j)
               line_glyphs[j] = glyphs_info.glyphs[glyph_start + j].codepoint;

            auto text_blob = SkTextBlob::MakeFromPosTextH(
               line_glyphs.data()
               , line_glyphs.size() * sizeof(SkGlyphID)
               , positions.data(), 0
               , *_font.impl()
               , SkTextEncoding::kGlyphID
            );

            auto last_glyph = std::min<std::size_t>(glyph_count+1, positions.size());
            positions.erase(positions.begin()+last_glyph, positions.end());
            _rows.push_back(
               row_info{
                  point{ linfo.offset, y }
                  , line_width
                  , finfo.line_height
                  , std::size_t(glyph_count)
                  , std::size_t(glyph_start)
                  , text_blob
                  , std::move(positions)
               }
            );

            positions.clear();
            glyph_start = glyph_idx + 1;
            y += finfo.line_height;
            linfo = glf(y);
            x = 0;
         };

      for (std::size_t glyph_idx = 0; glyph_idx != glyphs_info.count; ++glyph_idx)
      {
         positions.push_back(x + (glyphs_info.positions[glyph_idx].x_offset * scalex));
         x += glyphs_info.positions[glyph_idx].x_advance * scalex;
         auto idx = glyphs_info.glyphs[glyph_idx].cluster;
         bool indeterminate = _breaks[idx].line == indeterminate;

         if (_breaks[idx].line == must_break || indeterminate)
         {
            // We got a hard-break or we are at the end, so must break now
            new_line(idx, glyph_idx, true, indeterminate);
         }
         else if (x > linfo.width)
         {
            // We break the line when x exceeds the target width
            std::size_t len = positions.size();
            auto start_line = glyphs_info.glyphs[glyph_start].cluster;
            auto end_line = glyphs_info.glyphs[glyph_start+len].cluster;
            bool force_break = true;

            for (int i = end_line-1; i >= static_cast<int>(start_line); --i)
            {
               if (_breaks[i].line == allow_break)
               {
                  new_line(i, glyph_idx, false, false);
                  force_break = false;
                  break;
               }
            }
            // deal with the case where we have to forcefully break the line
            if (force_break)
               new_line(end_line, glyph_idx, false, false);
         }
      }
      if (_rows.size())
      {
         auto& last = _rows.back();
         last.pos.y += finfo.last_line_height - finfo.line_height;
         last.height = finfo.last_line_height;
      }
   }

   void  text_layout::impl::draw(canvas& cnv, point p, color c)
   {
      _paint.setColor4f({ c.red, c.green, c.blue, c.alpha }, nullptr);
      if (_rows.size() == 0)
         return;

      for (auto const& line : _rows)
      {
         if (line.width > 0)
         {
            auto sk_cnv = cnv.impl();
            SkPaint paint;
            paint.setAntiAlias(true);
            paint.setStyle(SkPaint::kFill_Style);
            sk_cnv->drawTextBlob(
               line.line
               , p.x+line.pos.x, p.y+line.pos.y
               , _paint
            );
         }
      }
   }

   point text_layout::impl::caret_point(std::size_t index) const
   {
      if (_rows.size() == 0)
         return { 0, 0 };

      auto glyphs_info = _buff.glyphs();

      // Find the glyph index from string index
      auto glyph_index = index;
      auto row_index = -1;
      if (index < _text.size())
      {
         glyph_index = glyphs_info.glyph_index(index);
      }
      else
      {
         glyph_index = glyphs_info.count;
         row_index = _rows.size() - 1;
      }

      // Find the row that includes the glyph index
      if (row_index == -1)
      {
         auto i = std::lower_bound(_rows.begin(), _rows.end(), glyph_index,
            [](auto const& row, std::size_t pos)
            {
               return (row.glyph_index + row.glyph_count) < pos;
            }
         );
         if (i == _rows.end())
            return { -1, -1 };
         row_index = i - _rows.begin();
      }

      // Now find the glyph position in the row
      auto const& row = _rows[row_index];
      auto pos = glyph_index - row.glyph_index;
      auto offset = (pos < row.positions.size())? row.positions[pos] : row.width;
      return { row.pos.x + offset, row.pos.y };
   }

   std::size_t text_layout::impl::caret_index(point p) const
   {
      if (_rows.size() == 0)
         return 0;

      auto glyphs_info = _buff.glyphs();
      auto i = std::lower_bound(_rows.begin(), _rows.end(), p.y,
         [](auto const& row, float y)
         {
            return row.pos.y < y;
         }
      );
      if (i == _rows.end())
         return npos;

      if (p.x <= i->pos.x)
         return glyphs_info.glyphs[i->glyph_index].cluster;

      auto is_last_row = (i == _rows.end()-1);
      if (!is_last_row && p.x >= (i->pos.x + i->width))
         return glyphs_info.glyphs[i->glyph_index + i->glyph_count].cluster;

      auto f = i->positions.begin();
      auto l = i->positions.end();
      auto j = std::lower_bound(f, l, p.x - i->pos.x,
         [](float pos, float x)
         {
            return pos < x;
         }
      );
      if (j == l)
         return is_last_row? _text.size() : npos;
      auto index = i->glyph_index + (j-f);
      return glyphs_info.glyphs[index].cluster;
   }

   std::size_t text_layout::impl::num_lines() const
   {
      return _rows.size();
   }

   class font& text_layout::impl::get_font()
   {
      return _font;
   }

   std::u32string_view text_layout::impl::get_text() const
   {
      return _text;
   }

   text_layout::break_enum text_layout::impl::line_break(std::size_t index) const
   {
      if (index >= _breaks.size())
         return indeterminate;
      return _breaks[index].line;
   }

   text_layout::break_enum text_layout::impl::word_break(std::size_t index) const
   {
      if (index >= _breaks.size())
         return indeterminate;
      return _breaks[index].line;
   }

   ////////////////////////////////////////////////////////////////////////////
   text_layout::text_layout(font_descr font_, std::string_view utf8)
    : _impl{ std::make_unique<impl>(font_, to_utf32(utf8)) }
   {
   }

   text_layout::text_layout(font_descr font_, std::u32string_view utf32)
    : _impl{ std::make_unique<impl>(font_, utf32) }
   {
   }

   text_layout::~text_layout()
   {
   }

   text_layout::text_layout(text_layout&& rhs) noexcept
    : _impl{ std::move(rhs._impl) }
   {
   }

   void text_layout::text(std::string_view utf8)
   {
      _impl = std::make_unique<impl>(_impl->get_font(), to_utf32(utf8));
   }

   void text_layout::text(std::u32string_view utf32)
   {
      _impl = std::make_unique<impl>(_impl->get_font(), utf32);
   }

   std::u32string_view text_layout::text() const
   {
      return _impl->get_text();
   }

   void text_layout::flow(float width, bool justify)
   {
      auto line_info_f = [width](float /*y*/)
      {
         return line_info{ 0, width };
      };

      auto lh = _impl->get_font().line_height();
      flow(line_info_f, { justify, lh, lh });
   }

   void text_layout::flow(get_line_info const& glf, flow_info finfo)
   {
      _impl->flow(glf, finfo);
   }

   void text_layout::draw(canvas& cnv, point p, color c) const
   {
      _impl->draw(cnv, p, c);
   }

   std::size_t text_layout::num_lines() const
   {
      return _impl->num_lines();
   }

   point text_layout::caret_point(std::size_t index) const
   {
      return _impl->caret_point(index);
   }

   std::size_t text_layout::caret_index(point p) const
   {
      return _impl->caret_index(p);
   }

   text_layout::break_enum text_layout::line_break(std::size_t index) const
   {
      return _impl->line_break(index);
   }

   text_layout::break_enum text_layout::word_break(std::size_t index) const
   {
      return _impl->word_break(index);
   }
}

