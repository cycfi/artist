/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#if !defined(ARTIST_DETAIL_ANALYTIC_SHADOW_NOVEMBER_25_2026)
#define ARTIST_DETAIL_ANALYTIC_SHADOW_NOVEMBER_25_2026

#include <artist/color.hpp>
#include <cstdint>
#include <memory>
#include <vector>

namespace cycfi::artist::detail
{
   ////////////////////////////////////////////////////////////////////////////
   // The shadow of a filled rectangle, rounded rectangle or circle, worked
   // out from the shape instead of by blurring its pixels. The blurred
   // coverage of such a shape is flat in the middle, varies only across
   // each side along it, and varies both ways only near the corners. So a
   // backend draws it in pieces: an alpha mask at each corner, one line of
   // a side stretched or repeated along it, and a plain fill in the
   // middle. Where the blur leaves no middle along an axis, the pieces
   // along it join into one.
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

   // Whole pixels, x0 to x1 by y0 to y1.
   struct pixel_box
   {
      int                  x0 = 0, y0 = 0, x1 = 0, y1 = 0;

      bool                 empty() const { return x0 >= x1 || y0 >= y1; }
   };

   // The grid a shadow is cut on: nx columns between the pixels x[0] to
   // x[nx], and ny rows between y[0] to y[ny]. Each is 3 (the middle one
   // is that axis's middle) or 1 (no middle), or 0 when the masks would be
   // too large and the shadow is not to be drawn this way.
   struct shadow_layout
   {
      int                  nx = 0, ny = 0;
      int                  x[4] = {}, y[4] = {};
   };

   // What a piece of a layout is. One in the middle column and the middle
   // row is a plain fill. One in the middle column only is the same all
   // the way across: a line running down, its first column of pixels as a
   // mask, stretched or repeated over it. One in the middle row only is a
   // line running across, its first row, the same way down it. Any other
   // is a mask.
   enum class shadow_piece
   {
      fill, line_down, line_across, mask
   };

   struct shadow_mask
   {
      using native_ptr = std::shared_ptr<void>;

      int                  width = 0, height = 0;
      int                  stride = 0;    // bytes per row, a multiple of 4
      std::vector<uint8_t> alpha;         // rows from the top

      // For a backend to keep, with the mask, the image it makes of it.
      mutable native_ptr   native;
   };

   // strength is the alpha the shadow is drawn at: a fainter one is laid
   // out closer to the shape, as far as it shows.
   shadow_layout           layout_shadow(
                              shadow_shape const& s, double sigma
                            , double strength = 1
                           );

   // The pieces of l in turn: f(kind, box), box being a pixel_box.
                           template <typename F>
   void                    for_each_piece(shadow_layout const& l, F&& f);

   // The blurred coverage of s over the pixels from (x, y), w by h. The
   // result is good until the next call on the same thread.
   shadow_mask const&      shadow_mask_of(
                              shadow_shape const& s, double sigma
                            , int x, int y, int w, int h
                           );

   // The pixels a shape covers outright. Under an opaque fill, what of
   // its shadow lies there need not be drawn.
   pixel_box               hidden_part(shadow_shape const& shape);

   // What of b lies outside hide: up to four boxes, written to out.
   // Returns how many.
   int                     visible_parts(
                              pixel_box const& b, pixel_box const& hide
                            , pixel_box* out
                           );

   // Pixels of the color c at each coverage in alpha, n of them, step
   // bytes apart: premultiplied, alpha in the top byte, then red, green
   // and blue.
   void                    shadow_pixels(
                              uint32_t* out, uint8_t const* alpha
                            , int n, int step, color c
                           );

   ////////////////////////////////////////////////////////////////////////////
   // Inlines
   ////////////////////////////////////////////////////////////////////////////
   template <typename F>
   inline void for_each_piece(shadow_layout const& l, F&& f)
   {
      for (int j = 0; j != l.ny; ++j)
      {
         for (int i = 0; i != l.nx; ++i)
         {
            bool const mid_x = l.nx == 3 && i == 1;
            bool const mid_y = l.ny == 3 && j == 1;
            pixel_box const b{l.x[i], l.y[j], l.x[i + 1], l.y[j + 1]};
            if (b.empty())
               continue;

            if (mid_x && mid_y)
               f(shadow_piece::fill, b);
            else if (mid_x)
               f(shadow_piece::line_down, b);
            else if (mid_y)
               f(shadow_piece::line_across, b);
            else
               f(shadow_piece::mask, b);
         }
      }
   }
}

#endif
