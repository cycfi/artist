/*=============================================================================
   Copyright (c) 2021 Chance Snow, Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <artist/circle.hpp>
#include <artist/c/point.h>
#include <artist/c/rect.h>

using namespace cycfi;

extern "C" {
   ////////////////////////////////////////////////////////////////////////////
   // Circles
   ////////////////////////////////////////////////////////////////////////////
   using circle = artist::circle;

   rect        artist_circle_bounds(circle c) { return c.bounds(); }

   point       artist_circle_center(circle c) { return c.center(); }
   circle      artist_circle_inset(circle c, float x) { return c.inset(x); }
   circle      artist_circle_move(circle c, float dx, float dy) { return c.move(dx, dy); }
   circle      artist_circle_move_to(circle c, float x, float y) { return c.move_to(x, y); }
}
