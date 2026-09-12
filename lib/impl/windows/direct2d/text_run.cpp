/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]

   DirectWrite text_run: the per-paragraph layout backing for the shared
   text engine. Line/word break classes come from libunibreak (as in the
   Quartz/Cairo backends); wrapping + measurement + hit-testing use an
   IDWriteTextLayout. Caret indices are codepoint (u32) positions; DirectWrite
   speaks UTF-16, so we keep a u32->u16 offset map.
=============================================================================*/
#include <string_view>
#include <string>
#include <vector>
#include <algorithm>
#include <cmath>
#include <artist/text_run.hpp>
#include <artist/canvas.hpp>
#include <infra/utf8_utils.hpp>
#include "context.hpp"
#include <dwrite_1.h>      // IDWriteTextLayout1, for per-space justification
#include "font_impl.hpp"
#include "linebreak.h"
#include "wordbreak.h"

namespace cycfi::artist
{
   class text_run::impl
   {
   public:

      impl(font const& font_, std::u32string_view utf32);
      ~impl();

      using break_enum = text_run::break_enum;

      struct break_info
      {
         break_enum           line : 4;
         break_enum           word : 4;
      };

      void                    flow(get_line_info const& glf, flow_info finfo);
      void                    draw(canvas& cnv, point p, color c);
      point                   caret_point(std::size_t index) const;
      std::size_t             caret_index(point p) const;

      std::size_t             num_lines() const;
      font&                   get_font();
      std::u32string const&   get_text() const;

      break_enum              line_break(std::size_t index) const;
      break_enum              word_break(std::size_t index) const;

   private:

      std::size_t             u16_of(std::size_t u32_index) const;
      std::size_t             u32_of(std::size_t u16_index) const;

      // One flowed line. Each row is its own IDWriteTextLayout, so a row can
      // take the width and offset that get_line_info gave for its own y: a
      // single layout over the whole text can only wrap to one width.
      struct row_info
      {
         IDWriteTextLayout*   layout = nullptr;   // owned
         UINT32               start16 = 0;        // first UTF-16 unit of the row
         UINT32               len16 = 0;          // units consumed, newline included
         UINT32               draw16 = 0;         // units in the layout, newline excluded
         UINT32               caret16 = 0;        // units a caret can address in the row
         float                offset = 0;
         float                y = 0;              // baseline, relative to the flow origin
      };

      void                    clear_rows();
      std::size_t             row_of(std::size_t index) const;

      class font              _font;
      std::u32string          _text;
      std::wstring            _wtext;
      std::vector<UINT32>     _u32_to_u16;     // size == _text.size()+1
      std::vector<break_info> _breaks;

      std::vector<row_info>   _rows;
      float                   _line_height = 0;
   };

   text_run::impl::impl(font const& font_, std::u32string_view utf32)
    : _font{font_}
    , _text{utf32}
    , _breaks{utf32.size(), break_info{}}
   {
      struct init_break_
      {
         init_break_() { init_linebreak(); init_wordbreak(); }
      };
      static init_break_ init;

      // u32 -> u16 offset map (codepoints above BMP take two UTF-16 units).
      _u32_to_u16.resize(_text.size() + 1);
      _wtext = d2d::to_utf16(std::u32string_view{_text});
      {
         UINT32 u16 = 0;
         for (std::size_t i = 0; i != _text.size(); ++i)
         {
            _u32_to_u16[i] = u16;
            u16 += (_text[i] > 0xFFFF)? 2 : 1;
         }
         _u32_to_u16[_text.size()] = u16;
      }

      if (!_text.empty())
      {
         std::string lbrks(_text.size(), 0);
         set_linebreaks_utf32((utf32_t const*)_text.data(), _text.size(), "", lbrks.data());
         std::string wbrks(_text.size(), 0);
         set_wordbreaks_utf32((utf32_t const*)_text.data(), _text.size(), "", wbrks.data());

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
   }

   text_run::impl::~impl()
   {
      clear_rows();
   }

   namespace
   {
      // DirectWrite leaves the last line of a layout ragged, and a row layout
      // is all last line, so a justified row is spaced by hand: the slack is
      // shared out over the row's spaces, as the Cairo backend does.
      void justify(IDWriteTextLayout* lay, std::wstring_view row, float target)
      {
         IDWriteTextLayout1* lay1 = nullptr;
         if (FAILED(lay->QueryInterface(
               __uuidof(IDWriteTextLayout1), reinterpret_cast<void**>(&lay1))) || !lay1)
            return;

         DWRITE_TEXT_METRICS tm{};
         if (SUCCEEDED(lay->GetMetrics(&tm))
            && tm.width > 0 && target > 0 && (tm.width / target) >= 0.9f)
         {
            std::vector<UINT32> spaces;
            for (UINT32 i = 0; i != UINT32(row.size()); ++i)
               if (row[i] == L' ')
                  spaces.push_back(i);

            if (!spaces.empty())
            {
               float extra = (target - tm.width) / float(spaces.size());
               for (auto i : spaces)
                  lay1->SetCharacterSpacing(0.0f, extra, 0.0f, DWRITE_TEXT_RANGE{i, 1});
            }
         }
         d2d::release(lay1);
      }
   }

   void text_run::impl::clear_rows()
   {
      for (auto& r : _rows)
         d2d::release(r.layout);
      _rows.clear();
   }

   std::size_t text_run::impl::row_of(std::size_t index) const
   {
      // The first row that does not end before index, so an index on a row
      // boundary belongs to the start of the following row.
      auto u16 = UINT32(u16_of(index));
      for (std::size_t r = 0; r != _rows.size(); ++r)
         if (u16 < _rows[r].start16 + _rows[r].len16)
            return r;
      return _rows.size() - 1;
   }

   std::size_t text_run::impl::u16_of(std::size_t u32_index) const
   {
      if (u32_index >= _u32_to_u16.size())
         return _u32_to_u16.empty()? 0 : _u32_to_u16.back();
      return _u32_to_u16[u32_index];
   }

   std::size_t text_run::impl::u32_of(std::size_t u16_index) const
   {
      // First codepoint whose u16 offset is >= u16_index.
      auto it = std::lower_bound(_u32_to_u16.begin(), _u32_to_u16.end(), UINT32(u16_index));
      if (it == _u32_to_u16.end())
         return _text.size();
      return std::size_t(it - _u32_to_u16.begin());
   }

   void text_run::impl::flow(get_line_info const& glf, flow_info finfo)
   {
      clear_rows();
      _line_height = finfo.line_height;

      auto fi = _font.impl();
      if (!fi || !fi->format || _wtext.empty())
         return;

      auto factory = d2d::dwrite_factory();
      UINT32 const total = UINT32(_wtext.size());
      UINT32 pos = 0;
      double y = 0;    // accumulated in double: line heights sum over many rows

      // Uniform line spacing with the baseline at the font ascent, so a row's
      // layout origin is its baseline minus the ascent and paragraph stacking
      // matches the engine's num_lines * line_height model.
      auto set_spacing =
         [&](IDWriteTextLayout* l)
         {
            l->SetLineSpacing(
               DWRITE_LINE_SPACING_METHOD_UNIFORM, finfo.line_height, fi->ascent);
         };

      while (pos < total)
      {
         auto li = glf(float(y));

         // How much of the remaining text fits on one line at this width?
         IDWriteTextLayout* probe = nullptr;
         if (FAILED(factory->CreateTextLayout(
               _wtext.c_str() + pos, total - pos, fi->format,
               li.width, FLT_MAX, &probe)) || !probe)
            break;
         set_spacing(probe);

         UINT32 count = 0;
         probe->GetLineMetrics(nullptr, 0, &count);
         std::vector<DWRITE_LINE_METRICS> lm(count? count : 1);
         if (count)
            probe->GetLineMetrics(lm.data(), count, &count);
         d2d::release(probe);

         UINT32 take = (count && lm[0].length)? lm[0].length : total - pos;
         UINT32 draw16 = take - lm[0].newlineLength;
         bool const last = (pos + take >= total);

         // A caret cannot land past the last visible glyph of a wrapped line:
         // the space that ended the line is the line's last caret position.
         UINT32 caret16 = last? take : take - lm[0].trailingWhitespaceLength;

         IDWriteTextLayout* lay = nullptr;
         if (FAILED(factory->CreateTextLayout(
               _wtext.c_str() + pos, draw16, fi->format,
               li.width, FLT_MAX, &lay)) || !lay)
            break;
         set_spacing(lay);
         // The row is one line by construction, and justification widens it
         // past the flow width, so wrapping must not reflow it.
         lay->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);

         if (finfo.justify && !last && lm[0].newlineLength == 0)
            justify(lay, std::wstring_view{_wtext}.substr(pos, caret16), li.width);

         _rows.push_back(
            row_info{lay, pos, take, draw16, caret16, li.offset, float(y)});

         pos += take;
         y += finfo.line_height;
      }

      // A text ending in a hard break opens an empty final line, so the caret
      // can land on it.
      if (!_rows.empty() && _rows.back().len16 > _rows.back().draw16)
      {
         auto li = glf(float(y));
         IDWriteTextLayout* lay = nullptr;
         if (SUCCEEDED(factory->CreateTextLayout(
               L"", 0, fi->format, li.width, FLT_MAX, &lay)) && lay)
         {
            set_spacing(lay);
            _rows.push_back(row_info{lay, total, 0, 0, 0, li.offset, float(y)});
         }
      }

      // The last line is given its own height, which moves its baseline down.
      if (!_rows.empty())
         _rows.back().y += finfo.last_line_height - finfo.line_height;
   }

   void text_run::impl::draw(canvas& cnv, point p, color c)
   {
      if (_rows.empty())
         return;
      auto ctx = reinterpret_cast<d2d::context*>(cnv.impl());
      auto t = ctx->target();
      if (!t)
         return;
      auto fi = _font.impl();
      if (!fi)
         return;

      auto m = ctx->state()? ctx->state()->current_matrix() : D2D1::Matrix3x2F::Identity();
      t->SetTransform(m);
      auto brush = d2d::make_paint(c, *t);

      // p is the first line's baseline; a layout is drawn from the top of its
      // line box, which the uniform spacing puts an ascent above the baseline.
      for (auto const& r : _rows)
         t->DrawTextLayout(
            D2D1::Point2F(p.x + r.offset, p.y + r.y - fi->ascent), r.layout,
            brush, D2D1_DRAW_TEXT_OPTIONS_NONE);

      d2d::release(brush);
   }

   point text_run::impl::caret_point(std::size_t index) const
   {
      if (_rows.empty())
         return {0, 0};

      auto const& r = _rows[row_of(index)];
      auto u16 = UINT32(u16_of(index));
      u16 = (u16 > r.start16)? u16 - r.start16 : 0;
      if (u16 > r.draw16)
         u16 = r.draw16;      // an index inside a trailing newline

      FLOAT px = 0, py = 0;
      DWRITE_HIT_TEST_METRICS hm{};
      r.layout->HitTestTextPosition(u16, FALSE, &px, &py, &hm);
      return {r.offset + px, r.y};
   }

   std::size_t text_run::impl::caret_index(point p) const
   {
      if (_rows.empty())
         return 0;

      // The shared engine's contract (text_layout.hpp): caret_index selects the
      // FIRST row whose baseline is at or below p.y, and returns npos for a
      // point below the last baseline.
      if (p.y > _rows.back().y + 0.01f)
         return npos;

      std::size_t row = _rows.size() - 1;
      for (std::size_t r = 0; r != _rows.size(); ++r)
      {
         if (_rows[r].y >= p.y - 0.01f)
         {
            row = r;
            break;
         }
      }
      auto const& ri = _rows[row];

      // At or left of the row's start, return its first code point (like the
      // Cairo backend's `p.x <= row.x` case). DirectWrite's HitTestPoint
      // otherwise skips a leading zero-width code point (e.g. U+2060 WORD
      // JOINER) to the first visible glyph.
      FLOAT sx = 0, sy = 0;
      DWRITE_HIT_TEST_METRICS shm{};
      ri.layout->HitTestTextPosition(0, FALSE, &sx, &sy, &shm);
      if (p.x - ri.offset <= sx + 0.01f)
         return u32_of(ri.start16);

      BOOL trailing = FALSE, inside = FALSE;
      DWRITE_HIT_TEST_METRICS hm{};
      ri.layout->HitTestPoint(
         p.x - ri.offset, _line_height * 0.5f, &trailing, &inside, &hm);
      std::size_t u16 = hm.textPosition + (trailing? hm.length : 0);
      if (u16 > ri.caret16)
         u16 = ri.caret16;
      return u32_of(ri.start16 + UINT32(u16));
   }

   std::size_t text_run::impl::num_lines() const
   {
      return _rows.size();
   }

   class font& text_run::impl::get_font()
   {
      return _font;
   }

   std::u32string const& text_run::impl::get_text() const
   {
      return _text;
   }

   text_run::break_enum text_run::impl::line_break(std::size_t index) const
   {
      if (index >= _breaks.size())
         return indeterminate;
      return _breaks[index].line;
   }

   text_run::break_enum text_run::impl::word_break(std::size_t index) const
   {
      if (index >= _breaks.size())
         return indeterminate;
      return _breaks[index].word;
   }

   ////////////////////////////////////////////////////////////////////////////
   text_run::text_run(font_descr font_, std::string_view utf8)
    : _impl{std::make_unique<impl>(font_, to_utf32(utf8))}
   {
   }

   text_run::text_run(font_descr font_, std::u32string_view utf32)
    : _impl{std::make_unique<impl>(font_, utf32)}
   {
   }

   text_run::~text_run()
   {
   }

   text_run::text_run(text_run&& rhs) noexcept
    : _impl{std::move(rhs._impl)}
   {
   }

   void text_run::text(std::string_view utf8)
   {
      _impl = std::make_unique<impl>(_impl->get_font(), to_utf32(utf8));
   }

   void text_run::text(std::u32string_view utf32)
   {
      _impl = std::make_unique<impl>(_impl->get_font(), utf32);
   }

   std::u32string_view text_run::text() const
   {
      return _impl->get_text();
   }

   void text_run::flow(float width, bool justify)
   {
      auto line_info_f = [width](float /*y*/) { return line_info{0, width}; };
      auto lh = _impl->get_font().line_height();
      _impl->flow(line_info_f, {justify, lh, lh});
   }

   void text_run::flow(get_line_info const& glf, flow_info finfo)
   {
      _impl->flow(glf, finfo);
   }

   void text_run::draw(canvas& cnv, point p, color c) const
   {
      _impl->draw(cnv, p, c);
   }

   std::size_t text_run::num_lines() const
   {
      return _impl->num_lines();
   }

   point text_run::caret_point(std::size_t index) const
   {
      return _impl->caret_point(index);
   }

   std::size_t text_run::caret_index(point p) const
   {
      return _impl->caret_index(p);
   }

   text_run::break_enum text_run::line_break(std::size_t index) const
   {
      return _impl->line_break(index);
   }

   text_run::break_enum text_run::word_break(std::size_t index) const
   {
      return _impl->word_break(index);
   }
}
