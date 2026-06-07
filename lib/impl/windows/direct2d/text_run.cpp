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

      class font              _font;
      std::u32string          _text;
      std::wstring            _wtext;
      std::vector<UINT32>     _u32_to_u16;     // size == _text.size()+1
      std::vector<break_info> _breaks;

      IDWriteTextLayout*      _layout = nullptr;
      float                   _offset_x = 0;
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
      d2d::release(_layout);
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
      d2d::release(_layout);
      _line_height = finfo.line_height;

      auto fi = _font.impl();
      if (!fi || !fi->format)
         return;

      auto l_info = glf(0);
      _offset_x = l_info.offset;

      if (FAILED(d2d::dwrite_factory()->CreateTextLayout(
            _wtext.c_str(), UINT32(_wtext.size()), fi->format,
            l_info.width, FLT_MAX, &_layout)) || !_layout)
         return;

      if (finfo.justify)
         _layout->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_JUSTIFIED);

      // Uniform line spacing so paragraph stacking matches the engine's
      // num_lines * line_height model; baseline at the font ascent.
      _layout->SetLineSpacing(
         DWRITE_LINE_SPACING_METHOD_UNIFORM, finfo.line_height, fi->ascent);
   }

   void text_run::impl::draw(canvas& cnv, point p, color c)
   {
      if (!_layout)
         return;
      auto ctx = reinterpret_cast<d2d::context*>(cnv.impl());
      auto t = ctx->target();
      if (!t)
         return;

      auto m = ctx->state()? ctx->state()->current_matrix() : D2D1::Matrix3x2F::Identity();
      t->SetTransform(m);
      auto brush = d2d::make_paint(c, *t);
      t->DrawTextLayout(
         D2D1::Point2F(p.x + _offset_x, p.y), _layout, brush,
         D2D1_DRAW_TEXT_OPTIONS_NONE);
      d2d::release(brush);
   }

   point text_run::impl::caret_point(std::size_t index) const
   {
      if (!_layout)
         return {0, 0};
      FLOAT px = 0, py = 0;
      DWRITE_HIT_TEST_METRICS hm{};
      _layout->HitTestTextPosition(UINT32(u16_of(index)), FALSE, &px, &py, &hm);
      return {_offset_x + px, py};
   }

   std::size_t text_run::impl::caret_index(point p) const
   {
      if (!_layout)
         return 0;

      // The shared engine's contract (text_layout.hpp): caret_index selects the
      // FIRST row whose top is >= p.y (top-based, like the Quartz/Cairo
      // lower_bound), and returns npos for a click below the last line. Use the
      // ACTUAL DirectWrite line metrics rather than a uniform line-height guess:
      // a fallback font (e.g. CJK glyphs in a Latin font) makes some lines taller
      // than the requested uniform height, and a fixed-lh guess then drifts and
      // mis-maps the row. (For uniform Latin text this is identical to r*lh.)
      UINT32 count = 0;
      _layout->GetLineMetrics(nullptr, 0, &count);
      if (count == 0)
         return 0;
      std::vector<DWRITE_LINE_METRICS> lm(count);
      if (FAILED(_layout->GetLineMetrics(lm.data(), count, &count)) || count == 0)
         return 0;

      // Per-line top y measured with the SAME API caret_point uses
      // (HitTestTextPosition on each line's first code unit). Deriving tops this
      // way keeps the top-based row selection exactly consistent with the y that
      // caret_point reports — mixing GetLineMetrics heights with
      // HitTestTextPosition tops drifts on fallback lines and mis-maps the row.
      std::vector<float> tops(count);
      std::vector<float> startx(count);   // x of each line's first code unit
      std::vector<UINT32> start16(count); // each line's first code unit
      UINT32 pos16 = 0;
      float last_bottom = 0;
      for (UINT32 r = 0; r != count; ++r)
      {
         FLOAT px = 0, py = 0;
         DWRITE_HIT_TEST_METRICS thm{};
         _layout->HitTestTextPosition(pos16, FALSE, &px, &py, &thm);
         tops[r] = py;
         startx[r] = px;
         start16[r] = pos16;
         last_bottom = py + lm[r].height;
         pos16 += lm[r].length;
      }

      if (p.y > last_bottom)
         return npos;   // below the last line

      // First row whose top is >= p.y (with a small tolerance for exact tops).
      UINT32 row = count;
      for (UINT32 r = 0; r != count; ++r)
      {
         if (tops[r] >= p.y - 0.01f)
         {
            row = r;
            break;
         }
      }
      if (row == count)         // p.y is inside the last line's body
         row = count - 1;

      // At or left of the line's start, return the line's first code point (like
      // the Cairo backend's `p.x <= row.x` case). DirectWrite's HitTestPoint
      // otherwise skips a leading zero-width code point (e.g. U+2060 WORD JOINER)
      // to the first visible glyph.
      if (p.x - _offset_x <= startx[row] + 0.01f)
         return u32_of(start16[row]);

      // Hit-test the column at a y safely inside the chosen row.
      float yy = (row + 1 < count)?
         (tops[row] + tops[row + 1]) * 0.5f
       : tops[row] + lm[row].height * 0.5f;

      BOOL trailing = FALSE, inside = FALSE;
      DWRITE_HIT_TEST_METRICS hm{};
      _layout->HitTestPoint(p.x - _offset_x, yy, &trailing, &inside, &hm);
      std::size_t u16 = hm.textPosition + (trailing? hm.length : 0);
      return u32_of(u16);
   }

   std::size_t text_run::impl::num_lines() const
   {
      if (!_layout)
         return 1;
      DWRITE_TEXT_METRICS tm{};
      if (FAILED(_layout->GetMetrics(&tm)))
         return 1;
      return tm.lineCount;
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
