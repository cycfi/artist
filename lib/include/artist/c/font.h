/*=============================================================================
   Copyright (c) 2021 Chance Snow, Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#ifndef __ARTIST_FONT_H
#define __ARTIST_FONT_H

#include "strings.h"

#ifdef __cplusplus
extern "C" {
#endif

   enum artist_weight_enum
   {
      thin              = 10,
      extra_light       = 20,
      light             = 30,
      weight_normal     = 40,
      medium            = 50,
      semi_bold         = 60,
      bold              = 70,
      extra_bold        = 80,
      black             = 90,
      extra_black       = 95,
   };

   enum artist_slant_enum
   {
      slant_normal      = 0,
      italic            = 90,
      oblique           = 100
   };

   enum artist_stretch_enum
   {
      ultra_condensed   = 25,
      extra_condensed   = 31,
      condensed         = 38,
      semi_condensed    = 44,
      stretch_normal    = 50,
      semi_expanded     = 57,
      expanded          = 63,
      extra_expanded    = 75,
      ultra_expanded    = 100
   };

   typedef struct {
      string_view*      _families;
      float             _size;
      uint8_t           _weight;
      artist_slant_enum _slant;
      uint8_t           _stretch;
   } artist_font_descr;

   artist_font_descr     artist_font_descr_normal(artist_font_descr f);
   artist_font_descr     artist_font_descr_size(artist_font_descr f, float size_);

   artist_font_descr     artist_font_descr_weight(artist_font_descr f, artist_weight_enum w);
   artist_font_descr     artist_font_descr_thin(artist_font_descr f);
   artist_font_descr     artist_font_descr_extra_light(artist_font_descr f);
   artist_font_descr     artist_font_descr_light(artist_font_descr f);
   artist_font_descr     artist_font_descr_weight_normal(artist_font_descr f);
   artist_font_descr     artist_font_descr_medium(artist_font_descr f);
   artist_font_descr     artist_font_descr_semi_bold(artist_font_descr f);
   artist_font_descr     artist_font_descr_bold(artist_font_descr f);
   artist_font_descr     artist_font_descr_extra_bold(artist_font_descr f);
   artist_font_descr     artist_font_descr_black(artist_font_descr f);
   artist_font_descr     artist_font_descr_extra_black(artist_font_descr f);

   artist_font_descr     artist_font_descr_style(artist_font_descr f, artist_slant_enum s);
   artist_font_descr     artist_font_descr_slant_normal(artist_font_descr f);
   artist_font_descr     artist_font_descr_italic(artist_font_descr f);
   artist_font_descr     artist_font_descr_oblique(artist_font_descr f);

   artist_font_descr     artist_font_descr_stretch(artist_font_descr f, artist_stretch_enum s);
   artist_font_descr     artist_font_descr_ultra_condensed(artist_font_descr f);
   artist_font_descr     artist_font_descr_extra_condensed(artist_font_descr f);
   artist_font_descr     artist_font_descr_condensed(artist_font_descr f);
   artist_font_descr     artist_font_descr_semi_condensed(artist_font_descr f);
   artist_font_descr     artist_font_descr_stretch_normal(artist_font_descr f);
   artist_font_descr     artist_font_descr_semi_expanded(artist_font_descr f);
   artist_font_descr     artist_font_descr_expanded(artist_font_descr f);
   artist_font_descr     artist_font_descr_extra_expanded(artist_font_descr f);
   artist_font_descr     artist_font_descr_ultra_expanded(artist_font_descr f);

   typedef struct artist_font artist_font;

   artist_font*          artist_font_create();
   artist_font*          artist_font_create_with_descr(artist_font_descr descr);
   void           artist_font_destroy(artist_font* f);

   typedef struct {
      float       ascent;
      float       descent;
      float       leading;
   } artist_metrics_info;

   artist_metrics_info   artist_font_metrics(artist_font* f);
   float          artist_font_line_height(artist_font* f);
   float          artist_font_measure_text(artist_font* f, string_view* str);

#ifdef __cplusplus
}
#endif
#endif
