/*=============================================================================
   Copyright (c) 2016-2024 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
// HB5: Cairo text_layout using HarfBuzz-shaped runs.
//
// The entire text is shaped once in the impl constructor (UTF-32 input so that
// HarfBuzz cluster values are UTF-32 code-point indices).  flow() re-uses the
// shaped glyph array across calls, using the per-code-point break table from
// libunibreak for wrapping decisions.  draw() renders with cairo_show_glyphs.
//
// Data model mirrors the Skia backend:
//   _glyphs  — shaped glyph array (glyph IDs + advances, indexed by glyph pos)
//   _breaks  — per UTF-32 code point break info (indexed by glyph.cluster)
//   row_info — glyph range [glyph_start, glyph_start+glyph_count)
//
#include "cairo_private.hpp"
#include "cairo_text.hpp"
#include <artist/text_layout.hpp>
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
   ////////////////////////////////////////////////////////////////////////////
   class text_layout::impl
   {
   public:

      impl(font_descr const& fdesc, std::u32string_view utf32);
      ~impl() = default;

      using break_enum = text_layout::break_enum;

      void        flow(get_line_info const& glf, flow_info finfo);
      void        draw(canvas& cnv, point p, color c) const;
      point       caret_point(std::size_t index) const;
      std::size_t caret_index(point p) const;
      std::size_t num_lines() const;

      std::u32string_view get_text() const       { return _text; }
      font&               get_font()             { return _font; }
      font_descr const&   get_font_descr() const { return _fdesc; }

      break_enum  line_break(std::size_t index) const;
      break_enum  word_break(std::size_t index) const;

   private:

      struct break_info
      {
         break_enum  line : 4;
         break_enum  word : 4;
      };

      struct row_info
      {
         std::size_t         glyph_start;
         std::size_t         glyph_count;
         float               x;          // line x offset (from get_line_info)
         float               y;          // baseline y relative to flow origin
         float               width;      // rendered width after justification
         std::vector<float>  positions;  // x per glyph relative to row.x
      };

      // Index in _glyphs of the first glyph whose cluster == utf32_idx.
      // Searches backward from min(utf32_idx, size-1) (LTR clusters are
      // non-decreasing, so this converges quickly).
      // Returns npos if not found (utf32_idx is inside a ligature cluster).
      std::size_t glyph_index(std::size_t utf32_idx) const;

      font_descr                  _fdesc;
      font                        _font;
      std::u32string              _text;
      std::vector<shaped_glyph>   _glyphs;   // shaped once; reused by flow()
      std::vector<break_info>     _breaks;   // per UTF-32 code point
      std::vector<row_info>       _rows;
   };

   ////////////////////////////////////////////////////////////////////////////
   std::size_t text_layout::impl::glyph_index(std::size_t utf32_idx) const
   {
      if (_glyphs.empty()) return npos;
      for (int i = int(std::min(utf32_idx, _glyphs.size()-1)); i >= 0; --i)
         if (_glyphs[std::size_t(i)].cluster == utf32_idx)
            return std::size_t(i);
      return npos;
   }

   ////////////////////////////////////////////////////////////////////////////
   text_layout::impl::impl(font_descr const& fdesc, std::u32string_view utf32)
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

      if (_text.empty()) return;

      std::vector<char> lbrks(_text.size(), 0);
      set_linebreaks_utf32(
         reinterpret_cast<utf32_t const*>(_text.data()),
         _text.size(), "", lbrks.data());

      std::vector<char> wbrks(_text.size(), 0);
      set_wordbreaks_utf32(
         reinterpret_cast<utf32_t const*>(_text.data()),
         _text.size(), "", wbrks.data());

      for (std::size_t i = 0; i < _breaks.size(); ++i)
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
            case WORDBREAK_BREAK:   _breaks[i].word = allow_break;   break;
            case WORDBREAK_NOBREAK: _breaks[i].word = no_break;      break;
            default:                _breaks[i].word = indeterminate; break;
         }
      }

      // Shape the full text once.  The UTF-32 overload of shape_text gives
      // cluster values as UTF-32 code-point indices, matching _breaks indices.
      auto* fi = _font.impl();
      if (fi && fi->_hb_font)
         _glyphs = shape_text(fi->_hb_font.get(), fi->_size, std::u32string_view(_text)).glyphs;
   }

   ////////////////////////////////////////////////////////////////////////////
   void text_layout::impl::flow(get_line_info const& glf, flow_info finfo)
   {
      _rows.clear();
      if (_glyphs.empty()) return;

      auto linfo = glf(0);
      float target = linfo.width;
      float y = 0;
      float x = 0;
      std::size_t glyph_start = 0;
      constexpr std::size_t NONE = std::size_t(-1);
      std::size_t last_allow = NONE;
      std::vector<float> positions;
      positions.reserve(128);

      auto flush_row =
         [&](std::size_t visible_end, std::size_t next_start, bool must_brk)
         {
            std::size_t n = visible_end > glyph_start
               ? visible_end - glyph_start : 0;

            std::vector<float> row_pos(
               positions.begin(),
               positions.begin() + std::min(n, positions.size()));

            float line_width = 0;
            if (n > 0 && n <= positions.size())
               line_width = positions[n-1]
                  + _glyphs[glyph_start + n - 1].x_advance;

            if (finfo.justify && !must_brk
                && line_width > 0 && target > 0
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
                     if (k < row_pos.size()) row_pos[k] += off;
                     if (is_space(_text[_glyphs[glyph_start + k].cluster]))
                        off += extra;
                  }
                  line_width = target;
               }
            }

            _rows.push_back(row_info{
               glyph_start, n, linfo.offset, y, line_width,
               std::move(row_pos)});

            y += finfo.line_height;
            linfo = glf(y);
            target = linfo.width;
            glyph_start = next_start;
            last_allow = NONE;
            x = 0;
            positions.clear();
         };

      for (std::size_t g = 0; g < _glyphs.size(); ++g)
      {
         auto const& glyph = _glyphs[g];
         auto brk = _breaks[glyph.cluster].line;

         if (brk == must_break)
         {
            // Hard newline: flush visible glyphs before it; skip the newline glyph.
            flush_row(g, g + 1, true);
            continue;
         }

         if (brk == allow_break)
            last_allow = g;

         positions.push_back(x + glyph.x_offset);
         x += glyph.x_advance;

         if (x > target)
         {
            if (last_allow != NONE)
            {
               // Soft wrap: break at last_allow (typically a space glyph).
               // Carry glyphs after it with fresh positions for the new line.
               std::vector<float> carry;
               float cx = 0;
               for (std::size_t k = last_allow + 1; k <= g; ++k)
               {
                  carry.push_back(cx + _glyphs[k].x_offset);
                  cx += _glyphs[k].x_advance;
               }
               flush_row(last_allow, last_allow + 1, false);
               positions = std::move(carry);
               x = cx;
            }
            else
            {
               // No break opportunity: force-break before glyph g.
               flush_row(g, g, false);
               positions.push_back(glyph.x_offset);
               x = glyph.x_advance;
            }
         }
      }

      // Flush remaining glyphs as the last line.
      {
         std::size_t n = positions.size();
         float line_width = n > 0
            ? positions[n-1] + _glyphs[glyph_start + n - 1].x_advance
            : 0;
         _rows.push_back(row_info{
            glyph_start, n, linfo.offset, y, line_width,
            std::move(positions)});
      }

      if (!_rows.empty())
      {
         auto& last = _rows.back();
         last.y += finfo.last_line_height - finfo.line_height;
      }
   }

   ////////////////////////////////////////////////////////////////////////////
   void text_layout::impl::draw(canvas& cnv, point p, color c) const
   {
      if (_rows.empty()) return;

      cairo_t* ctx = cnv.impl();
      auto* fi = _font.impl();
      if (!fi || !fi->_scaled_font) return;

      cairo_save(ctx);

#ifdef __APPLE__
      if (fi->_face &&
          cairo_surface_get_type(cairo_get_target(ctx)) == CAIRO_SURFACE_TYPE_QUARTZ)
      {
         cairo_set_font_face(ctx, fi->_face);
         cairo_set_font_size(ctx, fi->_size);
         // DEBUG: verify the scaled font is non-default
         auto* sf_check = cairo_get_scaled_font(ctx);
         (void)sf_check;
      }
      else
      {
         cairo_set_scaled_font(ctx, fi->_scaled_font);
      }
#else
      cairo_set_scaled_font(ctx, fi->_scaled_font);
#endif
      cairo_set_source_rgba(ctx, c.red, c.green, c.blue, c.alpha);

      for (auto const& row : _rows)
      {
         if (row.glyph_count == 0) continue;

         std::vector<cairo_glyph_t> cg;
         cg.reserve(row.glyph_count);
         for (std::size_t j = 0; j < row.glyph_count; ++j)
         {
            auto const& g = _glyphs[row.glyph_start + j];
            float gx = j < row.positions.size() ? row.positions[j] : row.width;
            cg.push_back({
               g.codepoint,
               double(p.x + row.x + gx),
               double(p.y + row.y - g.y_offset)
            });
         }
         // cairo_show_glyphs on the Quartz CG backend requires the current
         // point to be set; without it glyphs are silently not rendered.
         cairo_move_to(ctx, cg.front().x, cg.front().y);
         cairo_show_glyphs(ctx, cg.data(), int(cg.size()));
      }

      cairo_restore(ctx);
   }

   ////////////////////////////////////////////////////////////////////////////
   point text_layout::impl::caret_point(std::size_t index) const
   {
      if (_rows.empty())
         return {0, 0};
      if (index > _text.size())
         index = _text.size();

      // End of text: right edge of last row.
      if (index == _text.size())
      {
         auto const& last = _rows.back();
         return {last.x + last.width, last.y};
      }

      // Find the glyph for this UTF-32 index.
      std::size_t gi = glyph_index(index);
      if (gi == npos)
      {
         // Inside a ligature cluster — advance to the next available glyph.
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
         if (gi >= row.glyph_start
             && gi < row.glyph_start + row.glyph_count)
         {
            std::size_t pos = gi - row.glyph_start;
            float gx = pos < row.positions.size()
               ? row.positions[pos] : row.width;
            return {row.x + gx, row.y};
         }
         // gi is a break character (space or newline) that terminated this row.
         // Its visual position is at the row's right edge.
         if (gi == row.glyph_start + row.glyph_count)
            return {row.x + row.width, row.y};
      }

      auto const& last = _rows.back();
      return {last.x + last.width, last.y};
   }

   ////////////////////////////////////////////////////////////////////////////
   std::size_t text_layout::impl::caret_index(point p) const
   {
      if (_rows.empty()) return 0;

      auto it = std::lower_bound(_rows.begin(), _rows.end(), p.y,
         [](row_info const& r, float y){ return r.y < y; });

      if (it == _rows.end())
         return npos;

      auto const& row = *it;
      bool is_last = (it == _rows.end() - 1);

      if (row.positions.empty())
         return (row.glyph_start < _glyphs.size())
            ? _glyphs[row.glyph_start].cluster : _text.size();

      if (p.x <= row.x)
         return (row.glyph_start < _glyphs.size())
            ? _glyphs[row.glyph_start].cluster : 0;

      if (!is_last && p.x >= row.x + row.width)
      {
         std::size_t end_gi = row.glyph_start + row.glyph_count;
         return (end_gi < _glyphs.size())
            ? _glyphs[end_gi].cluster : _text.size();
      }

      float local_x = p.x - row.x;
      auto f = row.positions.begin();
      auto l = row.positions.end();
      auto j = std::lower_bound(f, l, local_x);

      if (j == l)
      {
         if (is_last) return _text.size();
         std::size_t end_gi = row.glyph_start + row.glyph_count;
         return (end_gi < _glyphs.size())
            ? _glyphs[end_gi].cluster : _text.size();
      }

      if (j != f)
      {
         float half = ((*j) - *(j-1)) / 2.0f;
         if (local_x < *(j-1) + half)
            --j;
      }

      std::size_t gi = row.glyph_start + std::size_t(j - f);
      return (gi < _glyphs.size()) ? _glyphs[gi].cluster : _text.size();
   }

   ////////////////////////////////////////////////////////////////////////////
   std::size_t text_layout::impl::num_lines() const
   {
      return _rows.size();
   }

   text_layout::break_enum text_layout::impl::line_break(std::size_t index) const
   {
      if (index >= _breaks.size()) return indeterminate;
      return _breaks[index].line;
   }

   text_layout::break_enum text_layout::impl::word_break(std::size_t index) const
   {
      if (index >= _breaks.size()) return indeterminate;
      return _breaks[index].word;
   }

   ////////////////////////////////////////////////////////////////////////////
   // text_layout public interface
   ////////////////////////////////////////////////////////////////////////////

   text_layout::text_layout(font_descr font_, std::string_view utf8)
    : _impl{std::make_unique<impl>(font_, to_utf32(utf8))}
   {
   }

   text_layout::text_layout(font_descr font_, std::u32string_view utf32)
    : _impl{std::make_unique<impl>(font_, utf32)}
   {
   }

   text_layout::text_layout(text_layout&& rhs) noexcept
    : _impl{std::move(rhs._impl)}
   {
   }

   text_layout::~text_layout()
   {
   }

   void text_layout::text(std::string_view utf8)
   {
      _impl = std::make_unique<impl>(_impl->get_font_descr(), to_utf32(utf8));
   }

   void text_layout::text(std::u32string_view utf32)
   {
      _impl = std::make_unique<impl>(_impl->get_font_descr(), utf32);
   }

   std::u32string_view text_layout::text() const
   {
      return _impl->get_text();
   }

   void text_layout::flow(float width, bool justify)
   {
      auto linfo_f = [width](float){ return line_info{0, width}; };
      float lh = _impl->get_font().line_height();
      flow(linfo_f, {justify, lh, lh});
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
