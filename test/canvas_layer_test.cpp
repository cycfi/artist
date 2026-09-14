/*=============================================================================
   Copyright (c) 2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]

   canvas_layer: what is drawn into a layer is what drawing the layer puts on
   the canvas, and a layer keeps its content from one frame to the next.
=============================================================================*/
#include "test_support.hpp"
#include <artist/canvas_layer.hpp>

namespace
{
   struct rgba8
   {
      int r, g, b, a;
   };

   bool near(rgba8 x, rgba8 y, int tol = 3)
   {
      return std::abs(x.r - y.r) <= tol && std::abs(x.g - y.g) <= tol
         && std::abs(x.b - y.b) <= tol && std::abs(x.a - y.a) <= tol;
   }

   // pixels() is premultiplied B, G, R, A on every backend.
   rgba8 pixel_at(image const& img, int x, int y)
   {
      auto p = reinterpret_cast<std::uint8_t const*>(img.pixels());
      p += 4 * (y * int(img.bitmap_size().x) + x);
      return {p[2], p[1], p[0], p[3]};
   }

   auto constexpr red = rgba(255, 0, 0, 255);
   auto constexpr blue = rgba(0, 0, 255, 255);
   auto constexpr lime = rgba(0, 255, 0, 255);
}

TEST_CASE("canvas_layer draws what was drawn into it")
{
   image dst{extent{100, 100}};
   {
      offscreen_image ctx{dst};
      canvas cnv{ctx.context()};
      canvas_layer layer{cnv, extent{50, 50}};
      CHECK(layer.size().x == 50);
      CHECK(layer.size().y == 50);
      {
         canvas layer_cnv{layer.context()};
         layer_cnv.fill_style(red);
         layer_cnv.fill_rect(rect{0, 0, 50, 50});
      }
      cnv.draw(layer, point{25, 25});
   }

#if defined(ARTIST_RECORDING)
   REQUIRE(recorded(dst).size() == 1);
   CHECK(recorded(dst, 0).kind == recording::op::image);
#else
   CHECK(near(pixel_at(dst, 50, 50), {255, 0, 0, 255}));
   CHECK(near(pixel_at(dst, 10, 10), {0, 0, 0, 0}));
   CHECK(near(pixel_at(dst, 90, 90), {0, 0, 0, 0}));
#endif
}

TEST_CASE("canvas_layer keeps its content between frames")
{
   image dst{extent{60, 60}};
   {
      offscreen_image ctx{dst};
      canvas cnv{ctx.context()};
      canvas_layer layer{cnv, extent{60, 60}};
      {
         canvas layer_cnv{layer.context()};
         layer_cnv.fill_style(blue);
         layer_cnv.fill_rect(rect{0, 0, 30, 60});
      }
      cnv.draw(layer);
      {
         canvas layer_cnv{layer.context()};
         layer_cnv.fill_style(lime);
         layer_cnv.fill_rect(rect{30, 0, 60, 60});
      }
      cnv.draw(layer);
   }

#if defined(ARTIST_RECORDING)
   REQUIRE(recorded(dst).size() == 2);
   CHECK(recorded(dst, 1).kind == recording::op::image);
#else
   CHECK(near(pixel_at(dst, 15, 30), {0, 0, 255, 255}));
   CHECK(near(pixel_at(dst, 45, 30), {0, 255, 0, 255}));
#endif
}
