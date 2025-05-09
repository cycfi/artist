/*=============================================================================
   Copyright (c) 2021 Chance Snow, Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#ifndef __ARTIST_POINT_H
#define __ARTIST_POINT_H

#ifdef __cplusplus
extern "C" {
#endif

   ////////////////////////////////////////////////////////////////////////////
   // Points
   ////////////////////////////////////////////////////////////////////////////
   typedef struct {
      float x;
      float y;
   } point;

   ////////////////////////////////////////////////////////////////////////////
   // Sizes
   ////////////////////////////////////////////////////////////////////////////
   typedef point extent;

   // TODO: inline point artist_point_move(point src, float dx, float dy) { return src.move(dx, dy); }
   // TODO: inline point artist_point_move_to(point src, float x_, float y_) { return src.move_to(x_, y_); }
   // TODO: inline point artist_point_reflect(point src, point p) { return src.reflect(p); }

#ifdef __cplusplus
}
#endif
#endif
