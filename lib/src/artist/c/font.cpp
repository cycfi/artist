/*=============================================================================
   Copyright (c) 2021 Chance Snow, Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <artist/font.hpp>
#include <artist/c/font.h>

extern "C" {
   font_descr     artist_font_descr_normal(font_descr f) { return f.normal(); }
   font_descr     artist_font_descr_size(font_descr f, float size_) { return f.size(size_); }

   font_descr     artist_font_descr_weight(font_descr f, weight_enum w) { return f.weight(w); }
   font_descr     artist_font_descr_thin(font_descr f) { return f.thin(); }
   font_descr     artist_font_descr_extra_light(font_descr f) { return f.extra_light(); }
   font_descr     artist_font_descr_light(font_descr f) { return f.light(); }
   font_descr     artist_font_descr_weight_normal(font_descr f) { return f.weight_normal(); }
   font_descr     artist_font_descr_medium(font_descr f) { return f.medium(); }
   font_descr     artist_font_descr_semi_bold(font_descr f) { return f.semi_bold(); }
   font_descr     artist_font_descr_bold(font_descr f) { return f.bold(); }
   font_descr     artist_font_descr_extra_bold(font_descr f) { return f.extra_bold(); }
   font_descr     artist_font_descr_black(font_descr f) { return f.black(); }
   font_descr     artist_font_descr_extra_black(font_descr f) { return f.extra_black(); }

   font_descr     artist_font_descr_style(font_descr f, slant_enum s) { return f.style(s); }
   font_descr     artist_font_descr_slant_normal(font_descr f) { return f.slant_normal(); }
   font_descr     artist_font_descr_italic(font_descr f) { return f.italic(); }
   font_descr     artist_font_descr_oblique(font_descr f) { return f.oblique(); }

   font_descr     artist_font_descr_stretch(font_descr f, stretch_enum s) { return f.stretch(s); }
   font_descr     artist_font_descr_ultra_condensed(font_descr f) { return f.ultra_condensed(); }
   font_descr     artist_font_descr_extra_condensed(font_descr f) { return f.extra_condensed(); }
   font_descr     artist_font_descr_condensed(font_descr f) { return f.condensed(); }
   font_descr     artist_font_descr_semi_condensed(font_descr f) { return f.semi_condensed(); }
   font_descr     artist_font_descr_stretch_normal(font_descr f) { return f.stretch_normal(); }
   font_descr     artist_font_descr_semi_expanded(font_descr f) { return f.semi_expanded(); }
   font_descr     artist_font_descr_expanded(font_descr f) { return f.expanded(); }
   font_descr     artist_font_descr_extra_expanded(font_descr f) { return f.extra_expanded(); }
   font_descr     artist_font_descr_ultra_expanded(font_descr f) { return f.ultra_expanded(); }

   typedef struct artist::font font;

   font*          artist_font_create() { return new artist::font(); }
   font*          artist_font_create_with_descr(font_descr descr) { return new artist::font(descr); }
   void           artist_font_destroy(font* f) { delete f; }

   metrics_info   artist_font_metrics(font* f) { return f->metrics(); }
   float          artist_font_line_height(font* f) { return f->line_height(); }
   float          artist_font_measure_text(font* f, string_view* str) { return f->measure_text(*str); }
}
