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
   using point = artist::point;

   ////////////////////////////////////////////////////////////////////////////
   // Sizes
   ////////////////////////////////////////////////////////////////////////////
   using extent = artist::extent;

   inline point artist_point_move(point src, float dx, float dy) { return src.move(dx, dy); }
   inline point artist_point_move_to(point src, float x_, float y_) { return src.move_to(x_, y_); }
   inline point artist_point_move_reflect(point src, point p) { return src.move((artist::point) {p.x, p.y}); }

#ifdef __cplusplus
}
#endif
#endif
