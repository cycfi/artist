/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]

   The golden comparison against deliberate changes: the same drawing does
   not differ, nor does a change too small to see; a moved shape, a visible
   change of colour, a change of alpha alone and a single wrong pixel do.
=============================================================================*/
#include "test_support.hpp"

#if !defined(ARTIST_RECORDING)

namespace
{
   template <typename F>
   image render(F f)
   {
      image img{100, 100, 1};
      {
         offscreen_image ctx{img};
         canvas cnv{ctx.context()};
         f(cnv);
      }
      return img;
   }

   // A square of the given color on a dark background, dx to the right.
   image square(color c, float dx = 0)
   {
      return render([=](canvas& cnv)
      {
         cnv.fill_style(rgba(40, 40, 40, 255));
         cnv.fill_rect(0, 0, 100, 100);
         cnv.fill_style(c);
         cnv.fill_rect(20 + dx, 20, 40, 40);
      });
   }
}

TEST_CASE("golden: the same drawing does not differ", "[golden]")
{
   CHECK(golden_differences(square(colors::red), square(colors::red)) == 0);
}

TEST_CASE("golden: a change too small to see does not differ", "[golden]")
{
   // 5 of 255 levels of red.
   CHECK(golden_differences(square(rgba(255, 0, 0, 255)), square(rgba(250, 0, 0, 255))) == 0);
}

TEST_CASE("golden: a visible change of colour differs", "[golden]")
{
   // 20 of 255 levels of grey, over the whole square.
   auto n = golden_differences(square(rgba(100, 100, 100, 255)), square(rgba(120, 120, 120, 255)));
   CHECK(n > 1000);
}

TEST_CASE("golden: a moved shape differs", "[golden]")
{
   auto n = golden_differences(square(colors::red), square(colors::red, 4));
   CHECK(n > 100);
}

TEST_CASE("golden: a change of alpha alone differs", "[golden]")
{
   // Black on transparent at full and at half alpha: the same color, as a
   // shadow is, with only its alpha changed.
   auto black = [](float alpha)
   {
      return render([=](canvas& cnv)
      {
         cnv.fill_style(rgba(0, 0, 0, alpha));
         cnv.fill_rect(20, 20, 40, 40);
      });
   };
   CHECK(golden_differences(black(255), black(128)) > 1000);
}

TEST_CASE("golden: a single wrong pixel differs", "[golden]")
{
   auto a = square(colors::red);
   auto b = square(colors::red);
   b.pixels()[10 * 100 + 80] = 0xFFFFFFFF;   // white, in the background
   CHECK(golden_differences(a, b) == 1);
}

#endif
