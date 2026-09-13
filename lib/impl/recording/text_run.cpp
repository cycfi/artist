/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]

   The recording text_run. Shaping, line breaking, justification and caret
   mapping follow the Cairo backend exactly; drawing records one fill_text
   per laid-out line instead of showing glyphs.
=============================================================================*/
#include "recording_impl.hpp"
#include <artist/text_run.hpp>
#include <artist/canvas.hpp>
#include <infra/utf8_utils.hpp>
#include <infra/support.hpp>
#include <linebreak.h>
#include <wordbreak.h>
#include <algorithm>
#include <string>
#include <vector>

namespace cycfi::artist
{
   using recording::shaped_glyph;

   ////////////////////////////////////////////////////////////////////////////
   class text_run::impl
   {
   public:

      impl(font_descr const& fdesc, std::u32string_view utf32);

      using break_enum = text_run::break_enum;

      void                    flow(get_line_info const& glf, flow_info finfo);
      void                    draw(canvas& cnv, point p, color c) const;
      point                   caret_point(std::size_t index) const;
      std::size_t             caret_index(point p) const;
      std::size_t             num_lines() const         { return _rows.size(); }

      std::u32string_view     get_text() const          { return _text; }
      font&                   get_font()                { return _font; }
      font_descr const&       get_font_descr() const    { return _fdesc; }

      break_enum              line_break(std::size_t index) const;
      break_enum              word_break(std::size_t index) const;

   private:

      struct break_info
      {
         break_enum           line : 4;
         break_enum           word : 4;
      };

      struct row_info
      {
         std::size_t          glyph_start;
         std::size_t          glyph_count;
         float                x;          // line x offset (from get_line_info)
         float                y;          // baseline y relative to flow origin
         float                width;      // width after justification
         std::vector<float>   positions;  // x per glyph relative to row.x
      };

      // The first glyph whose cluster is utf32_idx, or npos inside a ligature.
      std::size_t             glyph_index(std::size_t utf32_idx) const;

      font_descr              _fdesc;
      font                    _font;
      std::u32string          _text;
      std::vector<shaped_glyph>
                              _glyphs;
      std::vector<break_info> _breaks;
      std::vector<row_info>   _rows;
   };

   std::size_t text_run::impl::glyph_index(std::size_t utf32_idx) const
   {
      if (_glyphs.empty())
         return npos;
      for (int i = int(std::min(utf32_idx, _glyphs.size() - 1)); i >= 0; --i)
         if (_glyphs[std::size_t(i)].cluster == utf32_idx)
            return std::size_t(i);
      return npos;
   }

   text_run::impl::impl(font_descr const& fdesc, std::u32string_view utf32)
    : _fdesc(fdesc)
    , _font(fdesc)
    , _text(utf32)
    , _breaks(utf32.size(), break_info{indeterminate, indeterminate})
   {
      struct init_lb_
      {
         init_lb_() { init_linebreak(); init_wordbreak(); }
      };
      static init_lb_ init;

      if (_text.empty())
         return;

      std::vector<char> lbrks(_text.size(), 0);
      set_linebreaks_utf32(
         reinterpret_cast<utf32_t const*>(_text.data()), _text.size(), "", lbrks.data());

      std::vector<char> wbrks(_text.size(), 0);
      set_wordbreaks_utf32(
         reinterpret_cast<utf32_t const*>(_text.data()), _text.size(), "", wbrks.data());

      for (std::size_t i = 0; i != _breaks.size(); ++i)
      {
         switch (lbrks[i])
         {
            case LINEBREAK_MUSTBREAK:  _breaks[i].line = must_break;    break;
            case LINEBREAK_ALLOWBREAK: _breaks[i].line = allow_break;   break;
            case LINEBREAK_NOBREAK:    _breaks[i].line = no_break;      break;
            default:                   _breaks[i].line = indeterminate; break;
         }
         switch (wbrks[i])
         {
            case WORDBREAK_BREAK:      _breaks[i].word = allow_break;   break;
            case WORDBREAK_NOBREAK:    _breaks[i].word = no_break;      break;
            default:                   _breaks[i].word = indeterminate; break;
         }
      }

      _glyphs = recording::shape(_font, std::u32string_view{_text}).glyphs;
   }

   void text_run::impl::flow(get_line_info const& glf, flow_info finfo)
   {
      _rows.clear();
      if (_glyphs.empty())
         return;

      auto linfo = glf(0);
      float target = linfo.width;
      double y = 0;     // accumulated in double: line heights sum over many rows
      float x = 0;
      std::size_t glyph_start = 0;
      constexpr std::size_t none = std::size_t(-1);
      std::size_t last_allow = none;
      std::vector<float> positions;
      positions.reserve(128);

      auto flush_row =
         [&](std::size_t visible_end, std::size_t next_start, bool must_brk)
         {
            std::size_t n = visible_end > glyph_start? visible_end - glyph_start : 0;
            std::vector<float> row_pos(
               positions.begin(), positions.begin() + std::min(n, positions.size()));

            float line_width = 0;
            if (n > 0 && n <= positions.size())
               line_width = positions[n - 1] + _glyphs[glyph_start + n - 1].x_advance;

            if (finfo.justify && !must_brk && line_width > 0 && target > 0
               && (line_width / target) >= 0.9f)
            {
               std::size_t nsp = 0;
               for (std::size_t k = 0; k < n; ++k)
                  if (is_space(_text[_glyphs[glyph_start + k].cluster]))
                     ++nsp;
               if (nsp > 0)
               {
                  float extra = (target - line_width) / float(nsp);
                  float off = 0;
                  for (std::size_t k = 0; k < n; ++k)
                  {
                     if (k < row_pos.size())
                        row_pos[k] += off;
                     if (is_space(_text[_glyphs[glyph_start + k].cluster]))
                        off += extra;
                  }
                  line_width = target;
               }
            }

            _rows.push_back(row_info{
               glyph_start, n, linfo.offset, float(y), line_width, std::move(row_pos)});

            y += finfo.line_height;
            linfo = glf(float(y));
            target = linfo.width;
            glyph_start = next_start;
            last_allow = none;
            x = 0;
            positions.clear();
         };

      for (std::size_t g = 0; g < _glyphs.size(); ++g)
      {
         auto const& gl = _glyphs[g];
         auto brk = _breaks[gl.cluster].line;

         if (brk == must_break)
         {
            // Hard newline: flush what came before it, and skip it.
            flush_row(g, g + 1, true);
            continue;
         }

         if (brk == allow_break)
            last_allow = g;

         positions.push_back(x + gl.x_offset);
         x += gl.x_advance;

         if (x > target)
         {
            if (last_allow != none)
            {
               // Soft wrap at last_allow. A space there is absorbed into the
               // break; anything else (a CJK character is its own break
               // opportunity) stays on this line so no glyph is dropped.
               bool space = is_space(_text[_glyphs[last_allow].cluster]);
               std::size_t visible_end = space? last_allow : last_allow + 1;
               std::vector<float> carry;
               float cx = 0;
               for (std::size_t k = last_allow + 1; k <= g; ++k)
               {
                  carry.push_back(cx + _glyphs[k].x_offset);
                  cx += _glyphs[k].x_advance;
               }
               flush_row(visible_end, last_allow + 1, false);
               positions = std::move(carry);
               x = cx;
            }
            else
            {
               // No break opportunity: break before glyph g.
               flush_row(g, g, false);
               positions.push_back(gl.x_offset);
               x = gl.x_advance;
            }
         }
      }

      // What remains is the last line.
      {
         std::size_t n = positions.size();
         float line_width = n > 0
            ? positions[n - 1] + _glyphs[glyph_start + n - 1].x_advance
            : 0;
         _rows.push_back(row_info{
            glyph_start, n, linfo.offset, float(y), line_width, std::move(positions)});
      }

      if (!_rows.empty())
         _rows.back().y += finfo.last_line_height - finfo.line_height;
   }

   void text_run::impl::draw(canvas& cnv, point p, color c) const
   {
      auto m = _font.metrics();
      for (auto const& row : _rows)
      {
         if (row.glyph_count == 0)
            continue;

         std::size_t first = _text.size(), last = 0;
         for (std::size_t j = 0; j != row.glyph_count; ++j)
         {
            std::size_t cl = _glyphs[row.glyph_start + j].cluster;
            first = std::min(first, cl);
            last = std::max(last, cl);
         }
         // A cluster spans up to the next glyph's cluster.
         std::size_t next = row.glyph_start + row.glyph_count;
         std::size_t end = next < _glyphs.size()
            ? std::max<std::size_t>(last + 1, _glyphs[next].cluster)
            : _text.size();
         auto line = std::u32string_view{_text}.substr(first, end - first);

         float x = p.x + row.x;
         float y = p.y + row.y;
         recording::record_text(
            cnv, recording::op::fill_text, to_utf8(line),
            rect{x, y - m.ascent, x + row.width, y + m.descent}, c
         );
      }
   }

   point text_run::impl::caret_point(std::size_t index) const
   {
      if (_rows.empty())
         return {0, 0};
      if (index > _text.size())
         index = _text.size();

      if (index == _text.size())
      {
         auto const& last = _rows.back();
         return {last.x + last.width, last.y};
      }

      std::size_t gi = glyph_index(index);
      if (gi == npos)
      {
         // Inside a ligature: take the next glyph.
         for (std::size_t g = 0; g < _glyphs.size(); ++g)
         {
            if (_glyphs[g].cluster > index)
            {
               gi = g;
               break;
            }
         }
         if (gi == npos)
         {
            auto const& last = _rows.back();
            return {last.x + last.width, last.y};
         }
      }

      for (auto const& row : _rows)
      {
         if (gi >= row.glyph_start && gi < row.glyph_start + row.glyph_count)
         {
            std::size_t pos = gi - row.glyph_start;
            float gx = pos < row.positions.size()? row.positions[pos] : row.width;
            return {row.x + gx, row.y};
         }
         // The space or newline that ended this row sits at its right edge.
         if (gi == row.glyph_start + row.glyph_count)
            return {row.x + row.width, row.y};
      }

      auto const& last = _rows.back();
      return {last.x + last.width, last.y};
   }

   std::size_t text_run::impl::caret_index(point p) const
   {
      if (_rows.empty())
         return 0;

      auto it = std::lower_bound(_rows.begin(), _rows.end(), p.y,
         [](row_info const& r, float y){ return r.y < y; });

      if (it == _rows.end())
         return npos;

      auto const& row = *it;
      bool is_last = (it == _rows.end() - 1);

      if (row.positions.empty())
         return row.glyph_start < _glyphs.size()? _glyphs[row.glyph_start].cluster : _text.size();

      if (p.x <= row.x)
         return row.glyph_start < _glyphs.size()? _glyphs[row.glyph_start].cluster : 0;

      if (!is_last && p.x >= row.x + row.width)
      {
         std::size_t end_gi = row.glyph_start + row.glyph_count;
         return end_gi < _glyphs.size()? _glyphs[end_gi].cluster : _text.size();
      }

      float local_x = p.x - row.x;
      auto f = row.positions.begin();
      auto l = row.positions.end();
      auto j = std::lower_bound(f, l, local_x);

      if (j == l)
      {
         if (is_last)
            return _text.size();
         std::size_t end_gi = row.glyph_start + row.glyph_count;
         return end_gi < _glyphs.size()? _glyphs[end_gi].cluster : _text.size();
      }

      if (j != f)
      {
         float half = ((*j) - *(j - 1)) / 2.0f;
         if (local_x < *(j - 1) + half)
            --j;
      }

      std::size_t gi = row.glyph_start + std::size_t(j - f);
      return gi < _glyphs.size()? _glyphs[gi].cluster : _text.size();
   }

   text_run::break_enum text_run::impl::line_break(std::size_t index) const
   {
      return index < _breaks.size()? _breaks[index].line : indeterminate;
   }

   text_run::break_enum text_run::impl::word_break(std::size_t index) const
   {
      return index < _breaks.size()? _breaks[index].word : indeterminate;
   }

   ////////////////////////////////////////////////////////////////////////////
   // text_run
   ////////////////////////////////////////////////////////////////////////////
   text_run::text_run(font_descr font_, std::string_view utf8)
    : _impl{std::make_unique<impl>(font_, to_utf32(utf8))}
   {
   }

   text_run::text_run(font_descr font_, std::u32string_view utf32)
    : _impl{std::make_unique<impl>(font_, utf32)}
   {
   }

   text_run::text_run(text_run&& rhs) noexcept
    : _impl{std::move(rhs._impl)}
   {
   }

   text_run::~text_run()
   {
   }

   void text_run::text(std::string_view utf8)
   {
      _impl = std::make_unique<impl>(_impl->get_font_descr(), to_utf32(utf8));
   }

   void text_run::text(std::u32string_view utf32)
   {
      _impl = std::make_unique<impl>(_impl->get_font_descr(), utf32);
   }

   std::u32string_view text_run::text() const
   {
      return _impl->get_text();
   }

   void text_run::flow(float width, bool justify)
   {
      auto linfo_f = [width](float){ return line_info{0, width}; };
      float lh = _impl->get_font().line_height();
      flow(linfo_f, {justify, lh, lh});
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
