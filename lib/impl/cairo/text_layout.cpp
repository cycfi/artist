/*=============================================================================
   Copyright (c) 2016-2024 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
// Stage 5: Cairo text_layout implementation.
//
// Uses cairo_scaled_font_text_to_glyphs() for per-glyph advance widths and
// libunibreak for line/word break opportunities.  Implements word-wrap and
// basic justification. Hit-testing (caret_index / caret_point) operates on
// the resulting per-line position tables.
//
// Limitations vs. Skia/HarfBuzz:
//   - No OpenType glyph substitution or kerning (plain FreeType advances).
//   - No bidirectional text.
//   - Multi-byte UTF-8 characters (non-ASCII) get approximate positions when
//     Cairo returns fewer glyphs than code points (ligatures, clusters).
//   - text_layout::draw() uses cairo_show_text per line (no SkTextBlob).
//
#include "cairo_private.hpp"
#include <artist/text_layout.hpp>
#include <artist/canvas.hpp>
#include <infra/utf8_utils.hpp>
#include <infra/support.hpp>
#include <linebreak.h>
#include <wordbreak.h>
#include <algorithm>
#include <cassert>
#include <cstring>
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

      std::u32string_view get_text() const { return _text; }
      font&               get_font()       { return _font; }

      break_enum  line_break(std::size_t index) const;
      break_enum  word_break(std::size_t index) const;

   private:

      // Per-character metadata derived once from the font.
      struct char_info
      {
         double x_advance;       // advance in user-space units
         break_enum line_brk;    // line-break opportunity at this position
         break_enum word_brk;    // word-break opportunity at this position
      };

      // Per-line layout result.
      struct row_info
      {
         std::size_t         start;     // UTF-32 index of first char on line
         std::size_t         end;       // UTF-32 index of char just past line
                                         // (may be '\n' or logical end)
         float               y;         // baseline y relative to flow origin
         float               width;     // rendered line width (after justify)
         std::vector<float>  positions; // x of each char relative to line left
      };

      void build_char_advances();
      void emit_row(std::size_t row_start, std::size_t visible_end,
                    bool must_brk, float& y, float target,
                    flow_info const& finfo,
                    std::vector<float> const& char_positions,
                    get_line_info const& glf);

      font                     _font;
      std::u32string           _text;
      std::string              _utf8;     // UTF-8 version used for Cairo calls
      std::vector<char_info>   _chars;    // per UTF-32 code point
      std::vector<row_info>    _rows;
   };

   ////////////////////////////////////////////////////////////////////////////
   // Helpers
   ////////////////////////////////////////////////////////////////////////////

   namespace
   {
      // Unicode Default_Ignorable_Code_Point: characters that should have zero
      // advance width. HarfBuzz handles these automatically; we must match it.
      bool is_default_ignorable(char32_t cp)
      {
         return (cp == 0x00AD)                         // SOFT HYPHEN
             || (cp >= 0x200B && cp <= 0x200F)         // ZWSP, ZWNJ, ZWJ, etc.
             || (cp >= 0x202A && cp <= 0x202E)         // BIDI marks
             || (cp >= 0x2060 && cp <= 0x2064)         // WORD JOINER, INVISIBLE OPERATORS
             || (cp >= 0x2066 && cp <= 0x206F)         // BIDI isolation, INHIBIT
             || (cp == 0xFEFF)                         // BOM / ZWNBSP
             || (cp >= 0xFE00 && cp <= 0xFE0F)         // VARIATION SELECTORS
             || (cp >= 0xFFF0 && cp <= 0xFFF8);        // specials
      }
   }

   ////////////////////////////////////////////////////////////////////////////
   // Build per-character advance widths from the scaled font.
   // Uses cairo_scaled_font_text_to_glyphs() with cluster information so that
   // multi-byte / multi-glyph clusters are handled.
   void text_layout::impl::build_char_advances()
   {
      _chars.clear();
      if (_text.empty()) return;

      _chars.resize(_text.size(), {0.0, indeterminate, indeterminate});

      cairo_scaled_font_t* sf = _font.impl()->_scaled_font;
      if (!sf)
         return; // no font — zero advances

      // --- libunibreak ---
      struct init_lb
      {
         init_lb() { init_linebreak(); init_wordbreak(); }
      };
      static init_lb lb_init;

      std::vector<char> lbrks(_text.size(), 0);
      set_linebreaks_utf32(
         reinterpret_cast<utf32_t const*>(_text.data()),
         _text.size(), "", lbrks.data());

      std::vector<char> wbrks(_text.size(), 0);
      set_wordbreaks_utf32(
         reinterpret_cast<utf32_t const*>(_text.data()),
         _text.size(), "", wbrks.data());

      for (std::size_t i = 0; i < _chars.size(); ++i)
      {
         switch (lbrks[i])
         {
            case LINEBREAK_MUSTBREAK:  _chars[i].line_brk = must_break;  break;
            case LINEBREAK_ALLOWBREAK: _chars[i].line_brk = allow_break; break;
            case LINEBREAK_NOBREAK:    _chars[i].line_brk = no_break;    break;
            default:                   _chars[i].line_brk = indeterminate; break;
         }
         switch (wbrks[i])
         {
            case WORDBREAK_BREAK:   _chars[i].word_brk = allow_break; break;
            case WORDBREAK_NOBREAK: _chars[i].word_brk = no_break;    break;
            default:                _chars[i].word_brk = indeterminate; break;
         }
      }

      // --- glyph advances via Cairo ---
      cairo_glyph_t*           glyphs    = nullptr;
      int                      num_glyph = 0;
      cairo_text_cluster_t*    clusters  = nullptr;
      int                      num_clust = 0;
      cairo_text_cluster_flags_t cflags  = CAIRO_TEXT_CLUSTER_FLAG_BACKWARD;

      cairo_status_t st = cairo_scaled_font_text_to_glyphs(
         sf, 0.0, 0.0,
         _utf8.c_str(), int(_utf8.size()),
         &glyphs, &num_glyph,
         &clusters, &num_clust,
         &cflags);

      if (st != CAIRO_STATUS_SUCCESS || num_glyph == 0)
      {
         cairo_glyph_free(glyphs);
         cairo_text_cluster_free(clusters);
         return;
      }

      // Map each cluster (UTF-8 byte range → glyph range) back to a UTF-32
      // character index, then assign the cluster's total advance to the first
      // character in the cluster (remaining chars in a multi-char cluster get 0).
      std::size_t glyph_off = 0;
      std::size_t byte_off  = 0;
      std::size_t utf32_off = 0;

      bool backward = (cflags & CAIRO_TEXT_CLUSTER_FLAG_BACKWARD) != 0;
      // For backward clusters the glyph array runs right-to-left.
      // We do not support bidirectional text; treat as forward.
      (void)backward;

      for (int ci = 0; ci < num_clust; ++ci)
      {
         int nb = clusters[ci].num_bytes;
         int ng = clusters[ci].num_glyphs;

         // Advance of this cluster = position of next cluster's first glyph
         // minus position of this cluster's first glyph (or final advance for last).
         double advance = 0.0;
         if (ng > 0 && glyph_off < (std::size_t)num_glyph)
         {
            double x0 = glyphs[glyph_off].x;
            if (glyph_off + ng < (std::size_t)num_glyph)
               advance = glyphs[glyph_off + ng].x - x0;
            else
            {
               // Last cluster: use the text's total x_advance minus x0.
               cairo_text_extents_t tex;
               cairo_scaled_font_text_extents(sf, _utf8.c_str(), &tex);
               advance = tex.x_advance - x0;
            }
         }

         // How many UTF-32 code points does this cluster span?
         std::size_t byte_remaining = (std::size_t)nb;
         std::size_t cp_count = 0;
         std::size_t scan = byte_off;
         while (byte_remaining > 0 && scan < _utf8.size())
         {
            unsigned char b = (unsigned char)_utf8[scan];
            std::size_t len = (b < 0x80) ? 1 :
                              (b < 0xE0) ? 2 :
                              (b < 0xF0) ? 3 : 4;
            scan += len;
            byte_remaining -= std::min<std::size_t>(len, byte_remaining);
            ++cp_count;
         }
         if (cp_count == 0) cp_count = 1; // safety

         // Assign advance to first code point in cluster; rest get 0.
         if (utf32_off < _chars.size())
            _chars[utf32_off].x_advance = advance;
         for (std::size_t k = 1; k < cp_count && utf32_off + k < _chars.size(); ++k)
            _chars[utf32_off + k].x_advance = 0.0;

         glyph_off += ng;
         byte_off  += nb;
         utf32_off += cp_count;
      }

      cairo_glyph_free(glyphs);
      cairo_text_cluster_free(clusters);

      // Zero out advances for Unicode Default_Ignorable_Code_Points.
      // HarfBuzz treats these as zero-width; FreeType fonts may give them
      // non-zero advances (e.g. U+2060 WORD JOINER in Open Sans = 7.2px).
      for (std::size_t i = 0; i < _text.size(); ++i)
      {
         if (is_default_ignorable(_text[i]))
            _chars[i].x_advance = 0.0;
      }
   }

   ////////////////////////////////////////////////////////////////////////////
   // Emit one row into _rows and advance y.
   // visible_end: index of the break char (space/newline) that terminates this line.
   //              It is NOT included in the row's visible content.
   void text_layout::impl::emit_row(
      std::size_t row_start,
      std::size_t visible_end,
      bool must_brk,
      float& y,
      float target_width,
      flow_info const& finfo,
      std::vector<float> const& char_positions,
      get_line_info const& glf)
   {
      std::size_t n = (visible_end > row_start) ? (visible_end - row_start) : 0;

      std::vector<float> row_pos(char_positions.begin(),
         char_positions.begin() + std::min(n, char_positions.size()));

      float line_width = 0;
      if (n > 0 && n <= char_positions.size())
         line_width = char_positions[n-1] + float(_chars[row_start + n - 1].x_advance);

      // Justify: spread spaces when line is ≥90% full and not a hard break.
      if (finfo.justify && !must_brk && line_width > 0 && target_width > 0 &&
         (line_width / target_width) >= 0.9f)
      {
         std::size_t nsp = 0;
         for (std::size_t k = 0; k < n; ++k)
            if (is_space(_text[row_start + k])) ++nsp;
         if (nsp > 0)
         {
            float extra = (target_width - line_width) / float(nsp);
            float off = 0;
            for (std::size_t k = 0; k < n; ++k)
            {
               if (k < row_pos.size()) row_pos[k] += off;
               if (is_space(_text[row_start + k])) off += extra;
            }
            line_width = target_width;
         }
      }

      _rows.push_back(row_info{row_start, visible_end, y, line_width, std::move(row_pos)});

      y += finfo.line_height;
      glf(y); // keep the callback contract
   }

   void text_layout::impl::flow(get_line_info const& glf, flow_info finfo)
   {
      _rows.clear();
      if (_text.empty()) return;

      auto linfo = glf(0);
      float target = linfo.width;
      float y = 0;
      float x = 0;
      std::size_t line_start = 0;
      constexpr std::size_t NONE = std::size_t(-1);
      std::size_t last_allow = NONE; // index of last allow_break char seen
      std::vector<float> positions;  // x-positions from line_start, indexed by (i - line_start)
      positions.reserve(128);

      auto do_flush =
         [&](std::size_t visible_end, std::size_t next_start, bool must_brk)
         {
            emit_row(line_start, visible_end, must_brk, y, target, finfo, positions, glf);

            linfo = glf(y);
            target = linfo.width;
            line_start = next_start;
            last_allow = NONE;
            x = 0;
            positions.clear();
         };

      for (std::size_t i = 0; i < _text.size(); ++i)
      {
         auto brk = _chars[i].line_brk;

         if (brk == must_break)
         {
            // '\n' at i terminates the line; the newline itself is the break char.
            // Flush chars [line_start, i) as the visible content.
            // row.end = i (pointing to '\n'), next line starts at i+1.
            do_flush(i, i + 1, true);
            continue;
         }

         if (brk == allow_break)
            last_allow = i; // space at i is a potential break

         positions.push_back(x);
         x += float(_chars[i].x_advance);

         if (x > target)
         {
            if (last_allow != NONE)
            {
               // Break at the space (allow_break char).
               // Line: chars [line_start, last_allow), space at last_allow consumed.
               // Recompute positions for next line: [last_allow+1, i].
               std::vector<float> carry;
               float cx = 0;
               for (std::size_t k = last_allow + 1; k <= i; ++k)
               {
                  carry.push_back(cx);
                  cx += float(_chars[k].x_advance);
               }

               do_flush(last_allow, last_allow + 1, false);

               positions = std::move(carry);
               x = cx;
            }
            else
            {
               // Force break: no break opportunity found. Break before char i.
               do_flush(i, i, false);
               positions.push_back(0);
               x = float(_chars[i].x_advance);
            }
         }
      }

      // Flush any remaining text as the last line.
      emit_row(line_start, _text.size(), true, y, target, finfo, positions, glf);

      // Adjust last-row y to use last_line_height.
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
      cairo_scaled_font_t* sf = _font.impl()->_scaled_font;
      if (!sf) return;

      cairo_save(ctx);
      cairo_set_scaled_font(ctx, sf);
      cairo_set_source_rgba(ctx, c.red, c.green, c.blue, c.alpha);

      for (auto const& row : _rows)
      {
         std::size_t n = row.positions.size(); // number of visible chars
         if (n == 0) continue;

         // Build UTF-8 for the visible chars on this line.
         std::u32string_view line_u32(_text.data() + row.start, n);
         std::string line_utf8 = to_utf8(line_u32);
         if (line_utf8.empty()) continue;

         // Get the natural glyph array from Cairo so we can override positions
         // with the justified ones stored in row.positions.
         cairo_glyph_t*            glyphs    = nullptr;
         int                       num_glyph = 0;
         cairo_text_cluster_t*     clusters  = nullptr;
         int                       num_clust = 0;
         cairo_text_cluster_flags_t cflags   = CAIRO_TEXT_CLUSTER_FLAG_BACKWARD;

         cairo_status_t st = cairo_scaled_font_text_to_glyphs(
            sf, 0.0, 0.0,
            line_utf8.c_str(), int(line_utf8.size()),
            &glyphs, &num_glyph,
            &clusters, &num_clust, &cflags);

         if (st == CAIRO_STATUS_SUCCESS && num_glyph > 0)
         {
            // Walk clusters: set each glyph's position to the justified x, and
            // mark glyphs for default-ignorable code points (e.g. U+2060 WORD
            // JOINER) with glyph index 0 so they are skipped by Cairo.
            std::size_t glyph_off = 0;
            std::size_t byte_off  = 0;
            std::size_t char_off  = 0; // UTF-32 index relative to row.start

            for (int ci = 0; ci < num_clust && glyph_off < (std::size_t)num_glyph; ++ci)
            {
               int nb = clusters[ci].num_bytes;
               int ng = clusters[ci].num_glyphs;

               // Determine if the first code point of this cluster is ignorable.
               char32_t cp = (row.start + char_off < _text.size())
                  ? _text[row.start + char_off] : 0;
               bool ignorable = is_default_ignorable(cp);

               float jx = (char_off < row.positions.size())
                  ? row.positions[char_off] : row.width;

               for (int gi = 0; gi < ng; ++gi)
               {
                  std::size_t idx = glyph_off + gi;
                  if (idx >= (std::size_t)num_glyph) break;
                  if (ignorable)
                  {
                     // Index 0 is the .notdef glyph; set it to a well-known
                     // no-op by removing it from the rendered set via index 0
                     // — but simpler: just set num_glyph to skip it below.
                     // Instead, mark with a sentinel we filter out afterwards.
                     glyphs[idx].index = 0;
                     // Position it off-screen to be safe.
                     glyphs[idx].x = -1e6;
                     glyphs[idx].y = -1e6;
                  }
                  else
                  {
                     glyphs[idx].x = p.x + jx;
                     glyphs[idx].y = p.y + row.y;
                  }
               }

               // Count UTF-32 code points in this cluster's byte range.
               std::size_t rem = (std::size_t)nb;
               std::size_t scan = byte_off;
               std::size_t cp_count = 0;
               while (rem > 0 && scan < line_utf8.size())
               {
                  unsigned char b = (unsigned char)line_utf8[scan];
                  std::size_t len = (b < 0x80) ? 1 : (b < 0xE0) ? 2 : (b < 0xF0) ? 3 : 4;
                  scan += len;
                  rem -= std::min(len, rem);
                  ++cp_count;
               }
               if (cp_count == 0) cp_count = 1;

               glyph_off += (std::size_t)ng;
               byte_off  += (std::size_t)nb;
               char_off  += cp_count;
            }

            // Build a filtered array excluding the off-screen ignorable glyphs.
            std::vector<cairo_glyph_t> visible;
            visible.reserve(num_glyph);
            for (int gi = 0; gi < num_glyph; ++gi)
               if (glyphs[gi].x > -1e5)
                  visible.push_back(glyphs[gi]);

            if (!visible.empty())
               cairo_show_glyphs(ctx, visible.data(), int(visible.size()));
         }

         cairo_glyph_free(glyphs);
         cairo_text_cluster_free(clusters);
      }

      cairo_restore(ctx);
   }

   ////////////////////////////////////////////////////////////////////////////
   point text_layout::impl::caret_point(std::size_t index) const
   {
      if (_rows.empty())
         return {0, 0};

      // Clamp oversized index to end-of-text.
      if (index > _text.size())
         index = _text.size();

      // End-of-text: return last row's right edge.
      if (index == _text.size())
      {
         auto const& last = _rows.back();
         return {last.width, last.y};
      }

      // Walk rows to find which row this char belongs to.
      for (auto const& row : _rows)
      {
         // Visible chars: [row.start, row.end)
         if (index >= row.start && index < row.end)
         {
            auto pos = index - row.start;
            float x = (pos < row.positions.size()) ? row.positions[pos] : row.width;
            return {x, row.y};
         }
         // Break char: index == row.end → belongs conceptually to this row's right edge.
         if (index == row.end)
            return {row.width, row.y};
      }

      // Fallback.
      auto const& last = _rows.back();
      return {last.width, last.y};
   }

   ////////////////////////////////////////////////////////////////////////////
   std::size_t text_layout::impl::caret_index(point p) const
   {
      if (_rows.empty()) return 0;

      // Find the first row with pos.y >= p.y
      auto it = std::lower_bound(_rows.begin(), _rows.end(), p.y,
         [](row_info const& r, float y){ return r.y < y; });

      if (it == _rows.end())
         return npos;

      auto const& row = *it;

      if (row.positions.empty())
         return row.start;

      bool is_last = (it == _rows.end() - 1);

      // Before start of line.
      if (p.x <= 0)
         return row.start;

      // Past end of line (but not last line).
      if (!is_last && p.x >= row.width)
         return row.end;

      // Binary search within the line positions.
      auto f = row.positions.begin();
      auto l = row.positions.end();
      auto j = std::lower_bound(f, l, p.x);

      if (j == l)
         return is_last ? _text.size() : row.end;

      if (j != f)
      {
         auto half = ((*j) - *(j-1)) / 2.0f;
         if (p.x < *(j-1) + half)
            --j;
      }

      std::size_t idx = row.start + std::size_t(j - f);
      return std::min(idx, _text.size());
   }

   ////////////////////////////////////////////////////////////////////////////
   std::size_t text_layout::impl::num_lines() const
   {
      return _rows.size();
   }

   text_layout::break_enum text_layout::impl::line_break(std::size_t index) const
   {
      if (index >= _chars.size()) return indeterminate;
      return _chars[index].line_brk;
   }

   text_layout::break_enum text_layout::impl::word_break(std::size_t index) const
   {
      if (index >= _chars.size()) return indeterminate;
      return _chars[index].word_brk;
   }

   ////////////////////////////////////////////////////////////////////////////
   // Constructor — build the font and character advance table.
   text_layout::impl::impl(font_descr const& fdesc, std::u32string_view utf32)
    : _font(fdesc)
    , _text(utf32)
    , _utf8(to_utf8(utf32))
   {
      build_char_advances();
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
      // TODO(cairo): font_descr is not stored in impl; re-created with default descr.
      // Store font_descr in impl so these setters can reconstruct properly.
      _impl = std::make_unique<impl>(font_descr{}, to_utf32(utf8));
   }

   void text_layout::text(std::u32string_view utf32)
   {
      // TODO(cairo): See text(string_view) — same font_descr gap.
      _impl = std::make_unique<impl>(font_descr{}, utf32);
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
