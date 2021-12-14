/*=============================================================================
   Copyright (c) 2021 Chance Snow, Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#ifndef __ARTIST_CIRCLE_H
#define __ARTIST_CIRCLE_H

#include "point.h"
#include "rect.h"

#ifdef __cplusplus
extern "C" {
#endif

   ////////////////////////////////////////////////////////////////////////////
   // Circles
   ////////////////////////////////////////////////////////////////////////////
   typedef struct {
      float       cx;
      float       cy;
      float       radius;
   } artist_circle;

   rect        artist_circle_bounds(artist_circle c);

   point       artist_circle_center(artist_circle c);
   artist_circle      artist_circle_inset(artist_circle c, float x);
   artist_circle      artist_circle_move(artist_circle c, float dx, float dy);
   artist_circle      artist_circle_move_to(artist_circle c, float x, float y);

#ifdef __cplusplus
}
#endif
#endif
