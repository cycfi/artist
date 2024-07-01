/*=============================================================================
   Copyright (c) 2021 Chance Snow, Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#ifndef __ARTIST_COLOR_H
#define __ARTIST_COLOR_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

   ////////////////////////////////////////////////////////////////////////////
   // Colors
   ////////////////////////////////////////////////////////////////////////////
   typedef struct {
      float red;
      float green;
      float blue;
      float alpha;
   } artist_color;

   artist_color artist_color_rgb_u32(uint32_t rgb);
   artist_color artist_color_rgba_u32(uint32_t rgba);
   artist_color artist_color_rgb(uint8_t r, uint8_t g, uint8_t b);
   artist_color artist_color_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
   artist_color artist_color_hsl(float h, float s, float l);
   artist_color artist_color_opacity(artist_color src, float alpha_);
   artist_color artist_color_level(color src, float amount);

#ifdef __cplusplus
}
#endif
#endif
