/*=============================================================================
   Copyright (c) 2021 Chance Snow, Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#ifndef __ARTIST_FONT_H
#define __ARTIST_FONT_H

#include <artist/font.hpp>

#include "strings.h"

#ifdef __cplusplus
using namespace cycfi;
extern "C" {
#endif

   typedef artist::font_constants::weight_enum weight_enum;
   typedef artist::font_constants::slant_enum slant_enum;
   typedef artist::font_constants::stretch_enum stretch_enum;

   typedef struct artist::font_descr font_descr;

   font_descr     artist_font_descr_normal(font_descr f);
   font_descr     artist_font_descr_size(font_descr f, float size_);

   font_descr     artist_font_descr_weight(font_descr f, weight_enum w);
   font_descr     artist_font_descr_thin(font_descr f);
   font_descr     artist_font_descr_extra_light(font_descr f);
   font_descr     artist_font_descr_light(font_descr f);
   font_descr     artist_font_descr_weight_normal(font_descr f);
   font_descr     artist_font_descr_medium(font_descr f);
   font_descr     artist_font_descr_semi_bold(font_descr f);
   font_descr     artist_font_descr_bold(font_descr f);
   font_descr     artist_font_descr_extra_bold(font_descr f);
   font_descr     artist_font_descr_black(font_descr f);
   font_descr     artist_font_descr_extra_black(font_descr f);

   font_descr     artist_font_descr_style(font_descr f, slant_enum s);
   font_descr     artist_font_descr_slant_normal(font_descr f);
   font_descr     artist_font_descr_italic(font_descr f);
   font_descr     artist_font_descr_oblique(font_descr f);

   font_descr     artist_font_descr_stretch(font_descr f, stretch_enum s);
   font_descr     artist_font_descr_ultra_condensed(font_descr f);
   font_descr     artist_font_descr_extra_condensed(font_descr f);
   font_descr     artist_font_descr_condensed(font_descr f);
   font_descr     artist_font_descr_semi_condensed(font_descr f);
   font_descr     artist_font_descr_stretch_normal(font_descr f);
   font_descr     artist_font_descr_semi_expanded(font_descr f);
   font_descr     artist_font_descr_expanded(font_descr f);
   font_descr     artist_font_descr_extra_expanded(font_descr f);
   font_descr     artist_font_descr_ultra_expanded(font_descr f);

   typedef struct artist::font font;

   font*          artist_font_create();
   font*          artist_font_create_with_descr(font_descr descr);
   void           artist_font_destroy(font* f);

   typedef struct artist::font::metrics_info metrics_info;

   metrics_info   artist_font_metrics(font* f);
   float          artist_font_line_height(font* f);
   float          artist_font_measure_text(font* f, string_view* str);

#ifdef __cplusplus
}
#endif
#endif
