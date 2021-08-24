/*=============================================================================
   Copyright (c) 2021 Chance Snow, Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#ifndef __ARTIST_POINT_H
#define __ARTIST_POINT_H

#include <artist/point.hpp>

#ifdef __cplusplus
using namespace cycfi;
extern "C" {
#endif

   ////////////////////////////////////////////////////////////////////////////
   // Points
   ////////////////////////////////////////////////////////////////////////////
   typedef struct artist::point point;

   ////////////////////////////////////////////////////////////////////////////
   // Sizes
   ////////////////////////////////////////////////////////////////////////////
   typedef struct artist::extent extent;

   inline point artist_point_move(point src, float dx, float dy) { return src.move(dx, dy); }
   inline point artist_point_move_to(point src, float x_, float y_) { return src.move_to(x_, y_); }
   inline point artist_point_reflect(point src, point p) { return src.reflect(p); }

#ifdef __cplusplus
}
#endif
#endif
