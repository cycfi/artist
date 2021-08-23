#ifndef __ARTIST_CIRCLE_H
#define __ARTIST_CIRCLE_H

#include <artist/circle.hpp>

#include "point.h"
#include "rect.h"

#ifdef __cplusplus
using namespace cycfi;
extern "C" {
#endif

   ////////////////////////////////////////////////////////////////////////////
   // Circles
   ////////////////////////////////////////////////////////////////////////////
   using circle = artist::circle;

   rect        artist_circle_bounds(circle c) { return c.bounds(); }

   point       artist_circle_center(circle c) { return c.center(); }
   circle      artist_circle_inset(circle c, float x) { return c.inset(x); }
   circle      artist_circle_move(circle c, float dx, float dy) { return c.move(dx, dy); }
   circle      artist_circle_move_to(circle c, float x, float y) { return c.move_to(x, y); }

#ifdef __cplusplus
}
#endif
#endif
