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
   } circle;

   rect        artist_circle_bounds(circle c);

   point       artist_circle_center(circle c);
   circle      artist_circle_inset(circle c, float x);
   circle      artist_circle_move(circle c, float dx, float dy);
   circle      artist_circle_move_to(circle c, float x, float y);

#ifdef __cplusplus
}
#endif
#endif
