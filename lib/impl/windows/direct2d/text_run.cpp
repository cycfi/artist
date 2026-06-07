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

      // The shared engine's contract (text_layout.hpp): caret_index must select
      // the FIRST row whose top is >= p.y (top-based, like the Quartz/Cairo
      // lower_bound), not the row that geometrically contains p.y. DirectWrite's
      // HitTestPoint is body-based (row containing y), so map p.y to the target
      // row ourselves, then hit-test that row's vertical centre for the column.
      float lh = _line_height > 0? _line_height : 1.0f;
      long n = long(num_lines());
      long row = long(std::ceil(p.y / lh - 0.01f));   // first row with top >= y
      if (row < 0)
         row = 0;
      if (row > n - 1)
         row = n - 1;
      float yy = (float(row) + 0.5f) * lh;

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
