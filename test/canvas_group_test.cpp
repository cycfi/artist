/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]

   Canvas groups: what is drawn in a group is composited as one, at the
   group's opacity; overlapping shapes in it do not show through each
   other; a group saves and restores the state, and nests.
=============================================================================*/
#include "test_support.hpp"
#include <type_traits>

namespace
{
   struct rgba8
   {
      int r, g, b, a;
   };

   // pixels() is premultiplied B, G, R, A on every backend.
   rgba8 pixel_at(image const& img, int x, int y)
   {
      auto p = reinterpret_cast<std::uint8_t const*>(img.pixels());
      p += 4 * (y * int(img.bitmap_size().x) + x);
      return {p[2], p[1], p[0], p[3]};
   }

   template <typename F>
   void render(image& img, F f)
   {
      offscreen_image ctx{img};
      canvas cnv{ctx.context()};
      f(cnv);
   }

   bool near(int a, int b, int tol = 12) { return std::abs(a - b) <= tol; }
}

TEST_CASE("canvas group: composited as one, at its opacity", "[group]")
{
   // Two overlapping red squares on white, in a group at half opacity:
   // pink everywhere they cover, the overlap no darker than the rest.
   image img{100, 100, 1};
   render(img, [](canvas& cnv)
   {
      cnv.fill_style(colors::white);
      cnv.fill_rect(0, 0, 100, 100);
      auto g = cnv.new_group(0.5);
      cnv.fill_style(colors::red);
      cnv.fill_rect(10, 10, 50, 50);
      cnv.fill_rect(40, 40, 50, 50);
   });
#if !defined(ARTIST_RECORDING)
   auto one = pixel_at(img, 20, 20);
   auto both = pixel_at(img, 50, 50);
   auto none = pixel_at(img, 80, 20);
   CHECK(near(one.r, 255));
   CHECK(near(one.g, 128));
   CHECK(near(both.g, one.g, 4));                // not darker where they overlap
   CHECK(near(none.g, 255));
#endif
}

TEST_CASE("canvas group: saves and restores the state, and nests", "[group]")
{
   image img{100, 100, 1};
   render(img, [](canvas& cnv)
   {
      cnv.fill_style(colors::white);
      cnv.fill_rect(0, 0, 100, 100);
      cnv.fill_style(colors::blue);
      {
         auto outer = cnv.new_group(0.5);
         cnv.fill_style(colors::red);
         {
            auto inner = cnv.new_group(0.5);      // a quarter, in all
            cnv.fill_rect(10, 10, 30, 30);
         }
      }
      cnv.fill_rect(60, 60, 30, 30);              // blue again, opaque
   });
#if !defined(ARTIST_RECORDING)
   auto q = pixel_at(img, 25, 25);
   CHECK(near(q.r, 255));
   CHECK(near(q.g, 191));
   auto b = pixel_at(img, 75, 75);
   CHECK(b.b > 240);
   CHECK(b.r < 15);
#endif

   static_assert(!std::is_copy_constructible_v<canvas::group>);
   static_assert(std::is_move_constructible_v<canvas::group>);
}
