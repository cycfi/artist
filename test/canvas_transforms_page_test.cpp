/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]

   Asserts the claims of docs/modules/ROOT/pages/canvas/transforms.adoc. Each
   case names the page section it comes from.
=============================================================================*/
#include "test_support.hpp"
#include <numbers>

namespace
{
   // Run f on a canvas over an image of the given scale.
   template <typename F>
   void on_canvas(F f, float scale = 1)
   {
      image img{100, 50, scale};
      offscreen_image ctx{img};
      canvas cnv{ctx.context()};
      f(cnv);
   }

   bool near(point a, point b, float tol = 0.01f)
   {
      return std::abs(a.x - b.x) <= tol && std::abs(a.y - b.y) <= tol;
   }
}

TEST_CASE("canvas transforms: skew takes angles", "[transforms]")
{
   // sx is the angle by which y shears with x, sy the angle by which x
   // shears with y, as for affine_transform::skew.
   auto t = float(std::tan(0.5));

   on_canvas([&](canvas& cnv)
   {
      cnv.skew(0.5, 0);
      CHECK(near(cnv.user_to_device(point{1, 0}), {1, t}));
      CHECK(near(cnv.user_to_device(point{0, 1}), {0, 1}));
   });

   on_canvas([&](canvas& cnv)
   {
      cnv.skew(0, 0.5);
      CHECK(near(cnv.user_to_device(point{0, 1}), {t, 1}));
      CHECK(near(cnv.user_to_device(point{1, 0}), {1, 0}));
   });
}
