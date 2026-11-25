/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#if !defined(ARTIST_DETAIL_ANALYTIC_SHADOW_NOVEMBER_25_2026)
#define ARTIST_DETAIL_ANALYTIC_SHADOW_NOVEMBER_25_2026

#include <cstdint>
#include <vector>

namespace cycfi::artist::detail
{
   ////////////////////////////////////////////////////////////////////////////
   // The shadow of a filled rectangle, rounded rectangle or circle, worked
   // out from the shape instead of by blurring its pixels. The blurred
   // coverage of such a shape is flat in the middle, varies only across
   // each side along it, and varies both ways only near the corners. So a
   // backend draws it in pieces: an alpha mask at each corner, a gradient
   // along each side and a plain fill in the middle. Where the blur leaves
   // no middle along an axis, the pieces along it join into one.
   //
   // All measures are device pixels, with pixel centers at half integers.
   // sigma is the standard deviation of the Gaussian blur.
   ////////////////////////////////////////////////////////////////////////////

   // A rectangle with rounded corners, the radius at most half its shorter
   // side. A radius of 0 is a plain rectangle; a square with a radius of
   // half its side is a circle.
   struct shadow_shape
   {
      double               x0, y0, x1, y1;
      double               radius;
   };

   // The grid a shadow is cut on: nx columns between the pixels x[0] to
   // x[nx], and ny rows between y[0] to y[ny]. Each is 3 (the middle one
   // is that axis's middle) or 1 (no middle), or 0 when the masks would be
   // too large and the shadow is not to be drawn this way. A piece in the
   // middle column and the middle row is a plain fill. One in the middle
   // column only is a gradient down it, one in the middle row only is a
   // gradient across it, and any other is a mask.
   struct shadow_layout
   {
      int                  nx = 0, ny = 0;
      int                  x[4] = {}, y[4] = {};
      int                  reach = 0;     // how far the blur spreads
   };

   struct shadow_mask
   {
      int                  width = 0, height = 0;
      std::vector<uint8_t> alpha;         // width bytes a row, from the top
   };

   struct shadow_stop
   {
      float                offset;        // 0 to 1 along the gradient
      float                alpha;
   };

   constexpr int max_shadow_stops = 68;

   shadow_layout           layout_shadow(
                              shadow_shape const& s, double sigma
                           );

   // The blurred coverage of s over the pixels from (x, y), w by h. The
   // result is good until the next call on the same thread.
   shadow_mask const&      shadow_mask_of(
                              shadow_shape const& s, double sigma
                            , int x, int y, int w, int h
                           );

   // The stops of a gradient between the pixels from and to along an axis
   // on which the shape lies between a and b: the blurred coverage there.
   // Writes up to max_shadow_stops and returns how many.
   int                     shadow_stops(
                              double a, double b, double sigma
                            , int from, int to, shadow_stop* stops
                           );
}

#endif
