/*=============================================================================
   Copyright (c) 2016-2024 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]

   Private Cairo text-shaping types and helpers. NOT part of the public API.
   Include from Cairo backend implementation files only.
=============================================================================*/
#pragma once
#include <hb.h>
#include <string_view>
#include <vector>

namespace cycfi::artist
{
   ////////////////////////////////////////////////////////////////////////////
   // shaped_glyph — one HarfBuzz output glyph in user-space pixel units.
   //
   // codepoint : HarfBuzz glyph ID (== FreeType/Cairo glyph index for the
   //             same face — safe to pass directly to cairo_show_glyphs).
   // cluster   : UTF-32 source index of the first code point that produced
   //             this glyph. Used by text_layout for break-opportunity lookup
   //             and caret mapping.
   // x_advance : horizontal pen advance after drawing this glyph, in pixels.
   // x_offset  : horizontal draw offset from the current pen position (kern,
   //             mark attachment), in pixels.
   // y_offset  : vertical draw offset (positive = up in HarfBuzz convention),
   //             in pixels.
   ////////////////////////////////////////////////////////////////////////////
   struct shaped_glyph
   {
      uint32_t  codepoint;
      uint32_t  cluster;
      float     x_advance;
      float     x_offset;
      float     y_offset;
   };

   ////////////////////////////////////////////////////////////////////////////
   // shaped_run — result of shaping one contiguous LTR UTF-8 run.
   //
   // advance_x : total horizontal advance for the run (sum of x_advance),
   //             in pixels. Use this for text measurement.
   ////////////////////////////////////////////////////////////////////////////
   struct shaped_run
   {
      std::vector<shaped_glyph> glyphs;
      float                     advance_x = 0.0f;
   };

   ////////////////////////////////////////////////////////////////////////////
   // shape_text (UTF-32 overload) — shape a UTF-32 LTR run.
   //
   // Cluster values in the returned glyphs are UTF-32 code-point indices
   // (0-based into utf32), suitable for direct lookup into text_layout's
   // per-code-point break table.
   ////////////////////////////////////////////////////////////////////////////
   inline shaped_run shape_text(hb_font_t* hb_fnt, float font_size,
                                std::u32string_view utf32)
   {
      if (!hb_fnt || utf32.empty())
         return {};

      hb_buffer_t* buf = hb_buffer_create();

      hb_buffer_add_utf32(buf,
         reinterpret_cast<uint32_t const*>(utf32.data()),
         int(utf32.size()), 0, int(utf32.size()));

      hb_buffer_guess_segment_properties(buf);
      hb_shape(hb_fnt, buf, nullptr, 0);

      unsigned int         count;
      hb_glyph_info_t*     infos = hb_buffer_get_glyph_infos(buf, &count);
      hb_glyph_position_t* poses = hb_buffer_get_glyph_positions(buf, &count);

      int hb_scalex, hb_scaley;
      hb_font_get_scale(hb_fnt, &hb_scalex, &hb_scaley);
      float scale = (hb_scalex > 0) ? (font_size / float(hb_scalex)) : 1.0f;

      shaped_run run;
      run.glyphs.reserve(count);
      float pen = 0.0f;
      for (unsigned i = 0; i < count; ++i)
      {
         shaped_glyph g;
         g.codepoint = infos[i].codepoint;
         g.cluster   = infos[i].cluster;
         g.x_advance = float(poses[i].x_advance) * scale;
         g.x_offset  = float(poses[i].x_offset)  * scale;
         g.y_offset  = float(poses[i].y_offset)   * scale;
         run.glyphs.push_back(g);
         pen += g.x_advance;
      }
      run.advance_x = pen;

      hb_buffer_destroy(buf);
      return run;
   }

   ////////////////////////////////////////////////////////////////////////////
   // shape_text — shape a UTF-8 LTR run with HarfBuzz.
   //
   // hb_fnt    : HarfBuzz font previously created via hb_ft_font_create_referenced
   //             with scale set to (upem, upem) — see font.cpp.
   // font_size : font size in user-space pixels (font_descr._size).
   // utf8      : text to shape (need not be NUL-terminated; length is used).
   //
   // Scale conversion: HB positions are in font units (at upem scale).
   //   pixel_value = hb_value * (font_size / hb_scale)
   // where hb_scale is read back via hb_font_get_scale to stay in sync with
   // whatever scale was actually set.
   //
   // Cluster values are UTF-32 code-point indices, assigned by HarfBuzz from
   // the buffer's cluster tracking. They are preserved verbatim in shaped_glyph
   // for use by text_layout's break-opportunity and caret-mapping logic.
   ////////////////////////////////////////////////////////////////////////////
   inline shaped_run shape_text(hb_font_t* hb_fnt, float font_size,
                                std::string_view utf8)
   {
      if (!hb_fnt || utf8.empty())
         return {};

      hb_buffer_t* buf = hb_buffer_create();

      // Feed UTF-8 bytes; HarfBuzz tracks cluster indices as UTF-32 code-point
      // offsets when using hb_buffer_add_utf8 with cluster_level == 0 (default).
      hb_buffer_add_utf8(buf, utf8.data(), int(utf8.size()), 0, int(utf8.size()));

      // Guess direction, script, and language from the text content.
      hb_buffer_guess_segment_properties(buf);

      hb_shape(hb_fnt, buf, nullptr, 0);

      unsigned int     count;
      hb_glyph_info_t*     infos = hb_buffer_get_glyph_infos(buf, &count);
      hb_glyph_position_t* poses = hb_buffer_get_glyph_positions(buf, &count);

      // Read back the scale that was set in font.cpp to compute the pixel factor.
      // hb_font_set_scale(upem, upem) was used, so hb_scalex == upem.
      int hb_scalex, hb_scaley;
      hb_font_get_scale(hb_fnt, &hb_scalex, &hb_scaley);
      float scale = (hb_scalex > 0) ? (font_size / float(hb_scalex)) : 1.0f;

      shaped_run run;
      run.glyphs.reserve(count);

      float pen_x = 0.0f;
      for (unsigned i = 0; i < count; ++i)
      {
         shaped_glyph g;
         g.codepoint = infos[i].codepoint;
         g.cluster   = infos[i].cluster;
         g.x_advance = float(poses[i].x_advance) * scale;
         g.x_offset  = float(poses[i].x_offset)  * scale;
         g.y_offset  = float(poses[i].y_offset)   * scale;
         run.glyphs.push_back(g);
         pen_x += g.x_advance;
      }
      run.advance_x = pen_x;

      hb_buffer_destroy(buf);
      return run;
   }
}
