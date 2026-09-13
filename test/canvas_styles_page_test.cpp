/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]

   Asserts the claims of docs/modules/ROOT/pages/canvas/styles.adoc. Each
   case names the page section it comes from. Every assertion holds on every
   backend: the styles behave as the W3C canvas API specifies.

   Every probe renders onto a transparent 100 by 100 image and samples
   pixels, so a pixel (x, y) covers [x, x+1) by [y, y+1). Sample points sit
   at least a pixel and a half from any edge, clear of the anti-aliasing,
   and "painted" and "empty" are alpha above 200 and below 30.
=============================================================================*/
#include "test_support.hpp"
#include <cmath>
#include <functional>
#include <limits>
#include <numbers>

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

   bool painted(image const& img, int x, int y)
   {
      return pixel_at(img, x, y).a > 200;
   }

   bool empty(image const& img, int x, int y)
   {
      return pixel_at(img, x, y).a < 30;
   }

   template <typename F>
   void render(image& img, F f)
   {
      offscreen_image ctx{img};
      canvas cnv{ctx.context()};
      f(cnv);
   }

   color const red = rgba(255, 0, 0, 255);
   color const blue = rgba(0, 0, 255, 255);
   rgba8 const red8 = {255, 0, 0, 255};
   rgba8 const blue8 = {0, 0, 255, 255};
   rgba8 const black8 = {0, 0, 0, 255};

   auto const not_a_number = std::numeric_limits<float>::quiet_NaN();
   auto const infinite_width = std::numeric_limits<float>::infinity();

   void hline(canvas& cnv, float x1, float x2, float y)
   {
      cnv.move_to(x1, y);
      cnv.line_to(x2, y);
      cnv.stroke();
   }
}

TEST_CASE("canvas styles: Paint", "[styles]")
{
   {
      // The default fill style is opaque black.
      image img{100, 100, 1};
      render(img, [](canvas& cnv) { cnv.fill_rect(10, 10, 20, 20); });
#if defined(ARTIST_RECORDING)
      REQUIRE(recorded(img).size() == 1);
      CHECK(recorded(img, 0).kind == recording::op::fill);
      CHECK(same_color(recorded(img, 0).paint, colors::black));
      CHECK(same_rect(recorded(img, 0).geometry, {10, 10, 30, 30}));
#else
      CHECK(near(pixel_at(img, 20, 20), black8));
#endif
   }

   {
      // Fill and stroke are two separate styles.
      image img{100, 100, 1};
      render(img, [](canvas& cnv)
      {
         cnv.fill_style(red);
         cnv.stroke_style(blue);
         cnv.line_width(10);
         cnv.add_rect(20, 20, 60, 60);
         cnv.fill_preserve();
         cnv.stroke();
      });
#if defined(ARTIST_RECORDING)
      REQUIRE(recorded(img).size() == 2);
      CHECK(recorded(img, 0).kind == recording::op::fill);
      CHECK(same_color(recorded(img, 0).paint, red));
      CHECK(same_rect(recorded(img, 0).geometry, {20, 20, 80, 80}));
      CHECK(recorded(img, 1).kind == recording::op::stroke);
      CHECK(same_color(recorded(img, 1).paint, blue));
      CHECK(recorded(img, 1).line_width == 10);
      CHECK(same_rect(recorded(img, 1).geometry, {15, 15, 85, 85}));
#else
      CHECK(near(pixel_at(img, 50, 50), red8));
      CHECK(near(pixel_at(img, 20, 50), blue8));
#endif
   }

   {
      // fill_color and stroke_color are the same operations.
      image img{100, 100, 1};
      render(img, [](canvas& cnv)
      {
         cnv.fill_color(red);
         cnv.stroke_color(blue);
         cnv.line_width(10);
         cnv.add_rect(20, 20, 60, 60);
         cnv.fill_preserve();
         cnv.stroke();
      });
#if defined(ARTIST_RECORDING)
      REQUIRE(recorded(img).size() == 2);
      CHECK(recorded(img, 0).kind == recording::op::fill);
      CHECK(same_color(recorded(img, 0).paint, red));
      CHECK(same_rect(recorded(img, 0).geometry, {20, 20, 80, 80}));
      CHECK(recorded(img, 1).kind == recording::op::stroke);
      CHECK(same_color(recorded(img, 1).paint, blue));
      CHECK(recorded(img, 1).line_width == 10);
      CHECK(same_rect(recorded(img, 1).geometry, {15, 15, 85, 85}));
#else
      CHECK(near(pixel_at(img, 50, 50), red8));
      CHECK(near(pixel_at(img, 20, 50), blue8));
#endif
   }

   {
      // A color replaces a gradient, and the other way round.
      canvas::linear_gradient gr{0, 0, 100, 0};
      gr.add_color_stop(0, blue);
      gr.add_color_stop(1, blue);

      image img{100, 100, 1};
      render(img, [&](canvas& cnv)
      {
         cnv.fill_style(gr);
         cnv.fill_style(red);
         cnv.fill_rect(0, 0, 50, 100);
         cnv.fill_style(gr);
         cnv.fill_rect(50, 0, 50, 100);
      });
#if defined(ARTIST_RECORDING)
      REQUIRE(recorded(img).size() == 2);
      CHECK(!recorded(img, 0).gradient);
      CHECK(same_color(recorded(img, 0).paint, red));
      CHECK(recorded(img, 1).gradient);
      CHECK(same_color(recorded(img, 1).paint, blue));
#else
      CHECK(near(pixel_at(img, 25, 50), red8));
      auto p = pixel_at(img, 75, 50);
      CHECK((p.b > 200 && p.r < 30 && p.a > 200));
#endif
   }
}

TEST_CASE("canvas styles: styles are saved state", "[styles]")
{
   image img{100, 100, 1};
   render(img, [](canvas& cnv)
   {
      cnv.fill_style(red);
      cnv.stroke_style(red);
      cnv.line_width(2);
      {
         auto s = cnv.new_state();
         cnv.fill_style(blue);
         cnv.stroke_style(blue);
         cnv.line_width(40);
         cnv.line_cap(canvas::square);
         cnv.shadow_style({0, 50}, 0, blue);
         cnv.composite_op(canvas::copy);
      }
      cnv.fill_rect(10, 10, 20, 20);
      hline(cnv, 50, 90, 50);
   });
#if defined(ARTIST_RECORDING)
   REQUIRE(recorded(img).size() == 2);
   auto const& fill = recorded(img, 0);
   CHECK(same_color(fill.paint, red));
   CHECK(fill.composite == canvas::source_over);
   CHECK(same_rect(fill.bounds, fill.geometry));  // no shadow
   auto const& line = recorded(img, 1);
   CHECK(same_color(line.paint, red));
   CHECK(line.line_width == 2);
   CHECK(line.line_cap == canvas::butt);
   CHECK(line.geometry.top == Approx(49));
   CHECK(line.geometry.bottom == Approx(51));
#else
   CHECK(near(pixel_at(img, 20, 20), red8));    // red, and not cleared
   CHECK(empty(img, 20, 70));                   // no shadow
   CHECK(pixel_at(img, 70, 49).r > 100);        // red stroke
   CHECK(empty(img, 70, 45));                   // 2 wide, not 40
   CHECK(empty(img, 46, 50));                   // butt, not square
#endif
}

TEST_CASE("canvas styles: Line Width", "[styles]")
{
   {
      image img{100, 100, 1};
      render(img, [](canvas& cnv)
      {
         cnv.line_width(10);
         hline(cnv, 10, 90, 50);
      });
#if defined(ARTIST_RECORDING)
      REQUIRE(recorded(img).size() == 1);
      CHECK(recorded(img, 0).line_width == 10);
      CHECK(recorded(img, 0).geometry.top == Approx(45));
      CHECK(recorded(img, 0).geometry.bottom == Approx(55));
#else
      CHECK(painted(img, 50, 46));
      CHECK(painted(img, 50, 53));
      CHECK(empty(img, 50, 43));
      CHECK(empty(img, 50, 56));
#endif
   }

   {
      // The width is in user space, under the transform in effect when the
      // path is stroked, not when the width is set.
      image img{100, 100, 1};
      render(img, [](canvas& cnv)
      {
         cnv.line_width(10);
         cnv.scale(2);
         hline(cnv, 5, 45, 25);
      });
#if defined(ARTIST_RECORDING)
      REQUIRE(recorded(img).size() == 1);
      CHECK(recorded(img, 0).line_width == 10);
      CHECK(recorded(img, 0).geometry.top == Approx(40));
      CHECK(recorded(img, 0).geometry.bottom == Approx(60));
#else
      CHECK(painted(img, 50, 42));
      CHECK(painted(img, 50, 57));
      CHECK(empty(img, 50, 37));
      CHECK(empty(img, 50, 62));
#endif
   }

   {
      // The default width is 1. A 1 unit line on y = 50 covers half of rows
      // 49 and 50.
      image img{100, 100, 1};
      render(img, [](canvas& cnv) { hline(cnv, 10, 90, 50); });
#if defined(ARTIST_RECORDING)
      REQUIRE(recorded(img).size() == 1);
      CHECK(recorded(img, 0).line_width == 1);
      CHECK(recorded(img, 0).geometry.top == Approx(49.5));
      CHECK(recorded(img, 0).geometry.bottom == Approx(50.5));
#else
      INFO("rows 49 and 50: " << pixel_at(img, 50, 49).a
         << ", " << pixel_at(img, 50, 50).a);
      CHECK(std::abs(pixel_at(img, 50, 49).a - 128) < 20);
      CHECK(std::abs(pixel_at(img, 50, 50).a - 128) < 20);
      CHECK(empty(img, 50, 47));
      CHECK(empty(img, 50, 52));
#endif
   }

   {
      // Zero, negative, infinite and NaN widths are ignored.
      image img{100, 100, 1};
      render(img, [](canvas& cnv)
      {
         cnv.line_width(10);
         cnv.line_width(0);
         cnv.line_width(-3);
         cnv.line_width(infinite_width);
         cnv.line_width(not_a_number);
         hline(cnv, 10, 90, 50);
      });
#if defined(ARTIST_RECORDING)
      REQUIRE(recorded(img).size() == 1);
      CHECK(recorded(img, 0).line_width == 10);
#else
      CHECK(painted(img, 50, 46));
      CHECK(empty(img, 50, 43));
#endif
   }
}

TEST_CASE("canvas styles: Line Cap", "[styles]")
{
   // A 20 wide line from x = 30 to x = 70 on y = 50.
   auto capped = [](image& img, int cap)
   {
      render(img, [cap](canvas& cnv)
      {
         cnv.line_width(20);
         if (cap >= 0)
            cnv.line_cap(canvas::line_cap_enum(cap));
         hline(cnv, 30, 70, 50);
      });
   };

   for (int cap : {-1, int(canvas::butt)})    // -1: the default
   {
      image img{100, 100, 1};
      capped(img, cap);
#if defined(ARTIST_RECORDING)
      CHECK(recorded(img, 0).line_cap == canvas::butt);
#else
      CHECK(painted(img, 32, 50));
      CHECK(empty(img, 27, 50));
#endif
   }

   {
      image img{100, 100, 1};
      capped(img, canvas::round);
#if defined(ARTIST_RECORDING)
      CHECK(recorded(img, 0).line_cap == canvas::round);
#else
      CHECK(painted(img, 22, 50));     // 7.5 from the endpoint
      CHECK(empty(img, 21, 41));       // 12 from it, past the half circle
#endif
   }

   {
      image img{100, 100, 1};
      capped(img, canvas::square);
#if defined(ARTIST_RECORDING)
      CHECK(recorded(img, 0).line_cap == canvas::square);
#else
      CHECK(painted(img, 22, 50));
      CHECK(painted(img, 21, 41));     // the square's corner
      CHECK(empty(img, 17, 50));
#endif
   }
}

TEST_CASE("canvas styles: Line Join", "[styles]")
{
   // A 40 wide right angle turning at (60, 50). Its miter corner is (80, 30)
   // and its bevel cuts from (60, 30) to (80, 50). (76, 33) is inside the
   // miter only; (73, 37) is inside the miter and the round join, but not
   // the bevel.
   auto joined = [](image& img, int join)
   {
      render(img, [join](canvas& cnv)
      {
         cnv.line_width(40);
         if (join >= 0)
            cnv.line_join(canvas::join_enum(join));
         cnv.move_to(10, 50);
         cnv.line_to(60, 50);
         cnv.line_to(60, 95);
         cnv.stroke();
      });
   };

   for (int join : {-1, int(canvas::miter_join)})   // -1: the default
   {
      image img{100, 100, 1};
      joined(img, join);
#if defined(ARTIST_RECORDING)
      CHECK(recorded(img, 0).line_join == canvas::miter_join);
#else
      CHECK(painted(img, 76, 33));
      CHECK(painted(img, 73, 37));
#endif
   }

   {
      image img{100, 100, 1};
      joined(img, canvas::bevel_join);
#if defined(ARTIST_RECORDING)
      CHECK(recorded(img, 0).line_join == canvas::bevel_join);
#else
      CHECK(empty(img, 76, 33));
      CHECK(empty(img, 73, 37));
#endif
   }

   {
      image img{100, 100, 1};
      joined(img, canvas::round_join);
#if defined(ARTIST_RECORDING)
      CHECK(recorded(img, 0).line_join == canvas::round_join);
#else
      CHECK(empty(img, 76, 33));
      CHECK(painted(img, 73, 37));
#endif
   }
}

TEST_CASE("canvas styles: miter_limit", "[styles]")
{
   // A 10 wide V turning at (50, 40) through 20 degrees. Its miter is
   // 1 / sin(10 degrees) = 5.76 line widths long, so its tip reaches
   // y = 11.2 under any limit above that, and the join is cut back to a
   // bevel at y = 39 under any limit below it. (50, 25) is inside the miter
   // only.
   auto vee = [](image& img, std::function<void(canvas&)> limit)
   {
      render(img, [&](canvas& cnv)
      {
         auto const a = 10 * std::numbers::pi_v<float> / 180;
         cnv.line_width(10);
         limit(cnv);
         cnv.move_to(50 - 55 * std::sin(a), 40 + 55 * std::cos(a));
         cnv.line_to(50, 40);
         cnv.line_to(50 + 55 * std::sin(a), 40 + 55 * std::cos(a));
         cnv.stroke();
      });
   };

   {
      // The default is 10.
      image img{100, 100, 1};
      vee(img, [](canvas&) {});
#if defined(ARTIST_RECORDING)
      CHECK(recorded(img, 0).miter_limit == 10);
#else
      CHECK(painted(img, 50, 25));
#endif
   }

   {
      // miter_limit() sets it back to 10.
      image img{100, 100, 1};
      vee(img, [](canvas& cnv)
      {
         cnv.miter_limit(2);
         cnv.miter_limit();
      });
#if defined(ARTIST_RECORDING)
      CHECK(recorded(img, 0).miter_limit == 10);
#else
      CHECK(painted(img, 50, 25));
#endif
   }

   for (float limit : {4.0f, 2.0f})
   {
      image img{100, 100, 1};
      vee(img, [limit](canvas& cnv) { cnv.miter_limit(limit); });
#if defined(ARTIST_RECORDING)
      CHECK(recorded(img, 0).miter_limit == limit);
#else
      CHECK(empty(img, 50, 25));
      CHECK(painted(img, 50, 45));
#endif
   }

   {
      // Zero, negative, infinite and NaN limits are ignored.
      image img{100, 100, 1};
      vee(img, [](canvas& cnv)
      {
         cnv.miter_limit(2);
         cnv.miter_limit(0);
         cnv.miter_limit(-1);
         cnv.miter_limit(infinite_width);
         cnv.miter_limit(not_a_number);
      });
#if defined(ARTIST_RECORDING)
      CHECK(recorded(img, 0).miter_limit == 2);
#else
      CHECK(empty(img, 50, 25));
#endif
   }
}

TEST_CASE("canvas styles: Shadow", "[styles]")
{
   {
      // The shadow is the shape, displaced, in the shadow color, under it.
      image img{100, 100, 1};
      render(img, [](canvas& cnv)
      {
         cnv.shadow_style({30, 0}, 0, blue);
         cnv.fill_style(red);
         cnv.fill_rect(10, 10, 20, 20);
      });
#if defined(ARTIST_RECORDING)
      REQUIRE(recorded(img).size() == 1);
      auto const& c = recorded(img, 0);
      CHECK(same_color(c.paint, red));
      CHECK(same_rect(c.geometry, {10, 10, 30, 30}));
      CHECK(c.shadow_offset == point{30, 0});
      CHECK(same_color(c.shadow_color, blue));
      CHECK(same_rect(c.bounds, {10, 10, 60, 30}));
#else
      CHECK(near(pixel_at(img, 20, 20), red8));
      CHECK(near(pixel_at(img, 50, 20), blue8));
      CHECK(empty(img, 70, 20));
#endif
   }

   {
      // Four scalars are the same call.
      image img{100, 100, 1};
      render(img, [](canvas& cnv)
      {
         cnv.shadow_style(30, 0, 0, blue);
         cnv.fill_style(red);
         cnv.fill_rect(10, 10, 20, 20);
      });
#if defined(ARTIST_RECORDING)
      CHECK(recorded(img, 0).shadow_offset == point{30, 0});
      CHECK(same_color(recorded(img, 0).shadow_color, blue));
#else
      CHECK(near(pixel_at(img, 50, 20), blue8));
#endif
   }

   {
      // Strokes cast a shadow too.
      image img{100, 100, 1};
      render(img, [](canvas& cnv)
      {
         cnv.shadow_style({0, 30}, 0, blue);
         cnv.stroke_style(red);
         cnv.line_width(10);
         hline(cnv, 10, 90, 20);
      });
#if defined(ARTIST_RECORDING)
      REQUIRE(recorded(img).size() == 1);
      CHECK(recorded(img, 0).kind == recording::op::stroke);
      CHECK(recorded(img, 0).shadow_offset == point{0, 30});
      CHECK(recorded(img, 0).bounds.bottom == Approx(55));
#else
      CHECK(near(pixel_at(img, 50, 50), blue8));
#endif
   }

   {
      // So do images.
      image pic{20, 20, 1};
      render(pic, [](canvas& cnv)
      {
         cnv.fill_style(blue);
         cnv.fill_rect(0, 0, 20, 20);
      });

      image img{100, 100, 1};
      render(img, [&](canvas& cnv)
      {
         cnv.shadow_style({30, 0}, 0, colors::black);
         cnv.draw(pic, point{10, 10});
      });
#if defined(ARTIST_RECORDING)
      REQUIRE(recorded(img).size() == 1);
      CHECK(recorded(img, 0).kind == recording::op::image);
      CHECK(same_rect(recorded(img, 0).geometry, {10, 10, 30, 30}));
      CHECK(recorded(img, 0).shadow_offset == point{30, 0});
      CHECK(same_rect(recorded(img, 0).bounds, {10, 10, 60, 30}));
#else
      CHECK(near(pixel_at(img, 20, 20), blue8));
      CHECK(near(pixel_at(img, 50, 20), black8));
#endif
   }

   {
      // With no offset, a blurred shadow is a glow around the shape.
      image img{100, 100, 1};
      render(img, [](canvas& cnv)
      {
         cnv.shadow_style(8, colors::black);
         cnv.fill_rect(40, 40, 20, 20);
      });
#if defined(ARTIST_RECORDING)
      CHECK(recorded(img, 0).shadow_blur == 8);
      CHECK(same_rect(recorded(img, 0).bounds, {32, 32, 68, 68}));
#else
      auto a = pixel_at(img, 36, 50).a;
      CHECK(a > 10);
      CHECK(a < 250);
#endif
   }

   {
      // A shadow under a fill with the even-odd rule.
      image img{100, 100, 1};
      render(img, [](canvas& cnv)
      {
         cnv.fill_rule(path::fill_odd_even);
         cnv.shadow_style({0, 50}, 0, blue);
         cnv.fill_style(red);
         cnv.fill_rect(10, 10, 20, 20);
      });
#if defined(ARTIST_RECORDING)
      REQUIRE(recorded(img).size() == 1);
      CHECK(same_color(recorded(img, 0).paint, red));
      CHECK(recorded(img, 0).shadow_offset == point{0, 50});
      CHECK(recorded(img, 0).bounds.bottom == Approx(80));
#else
      CHECK(near(pixel_at(img, 20, 20), red8));
      CHECK(near(pixel_at(img, 20, 70), blue8));
#endif
   }
}

TEST_CASE("canvas styles: shadow strength", "[styles]")
{
   {
      // The alpha of the shadow color scales the shadow.
      image img{100, 100, 1};
      render(img, [](canvas& cnv)
      {
         cnv.shadow_style({30, 0}, 0, rgba(0, 0, 0, 128));
         cnv.fill_rect(10, 10, 20, 20);
      });
#if defined(ARTIST_RECORDING)
      CHECK(same_color(recorded(img, 0).shadow_color, rgba(0, 0, 0, 128)));
#else
      CHECK(near(pixel_at(img, 50, 20), {0, 0, 0, 128}));
#endif
   }

   {
      // So does the alpha of what is drawn: a half-transparent shape casts a
      // half-strength shadow ...
      image img{100, 100, 1};
      render(img, [](canvas& cnv)
      {
         cnv.shadow_style({30, 0}, 0, colors::black);
         cnv.fill_style(rgba(255, 0, 0, 128));
         cnv.fill_rect(10, 10, 20, 20);
      });
#if defined(ARTIST_RECORDING)
      // The strength is rasterization; the journal holds its two factors.
      CHECK(same_color(recorded(img, 0).paint, rgba(255, 0, 0, 128)));
      CHECK(same_color(recorded(img, 0).shadow_color, colors::black));
#else
      CHECK(near(pixel_at(img, 50, 20), {0, 0, 0, 128}, 4));
#endif
   }

   {
      // ... a transparent one casts none ...
      image img{100, 100, 1};
      render(img, [](canvas& cnv)
      {
         cnv.shadow_style({30, 0}, 0, blue);
         cnv.fill_style(rgba(255, 0, 0, 0));
         cnv.fill_rect(10, 10, 20, 20);
      });
#if defined(ARTIST_RECORDING)
      CHECK(same_color(recorded(img, 0).paint, rgba(255, 0, 0, 0)));
#else
      CHECK(pixel_at(img, 50, 20).a == 0);
#endif
   }

   {
      // ... and so does a stroke ...
      image img{100, 100, 1};
      render(img, [](canvas& cnv)
      {
         cnv.shadow_style({0, 30}, 0, colors::black);
         cnv.stroke_style(rgba(0, 0, 255, 128));
         cnv.line_width(10);
         hline(cnv, 10, 90, 20);
      });
#if defined(ARTIST_RECORDING)
      CHECK(recorded(img, 0).kind == recording::op::stroke);
      CHECK(same_color(recorded(img, 0).paint, rgba(0, 0, 255, 128)));
#else
      CHECK(near(pixel_at(img, 50, 50), {0, 0, 0, 128}, 4));
#endif
   }

   {
      // ... and a gradient, point by point.
      canvas::linear_gradient gr{10, 0, 30, 0};
      gr.add_color_stop(0, red);
      gr.add_color_stop(1, red.opacity(0));

      image img{100, 100, 1};
      render(img, [&](canvas& cnv)
      {
         cnv.shadow_style({0, 50}, 0, blue);
         cnv.fill_style(gr);
         cnv.fill_rect(10, 10, 20, 20);
      });
#if defined(ARTIST_RECORDING)
      CHECK(recorded(img, 0).gradient);
      CHECK(same_color(recorded(img, 0).shadow_color, blue));
#else
      INFO("shadow alpha at x = 11 and 28: " << pixel_at(img, 11, 70).a
         << ", " << pixel_at(img, 28, 70).a);
      CHECK(pixel_at(img, 11, 70).a > 200);
      CHECK(pixel_at(img, 28, 70).a < 45);
#endif
   }
}

TEST_CASE("canvas styles: no shadow", "[styles]")
{
   {
      // A shadow with no offset and no blur is not drawn, so it does not
      // darken a half-transparent shape.
      image img{100, 100, 1};
      render(img, [](canvas& cnv)
      {
         cnv.shadow_style({0, 0}, 0, colors::black);
         cnv.fill_style(rgba(255, 0, 0, 128));
         cnv.fill_rect(10, 10, 20, 20);
      });
#if defined(ARTIST_RECORDING)
      CHECK(same_rect(recorded(img, 0).bounds, recorded(img, 0).geometry));
#else
      CHECK(near(pixel_at(img, 20, 20), {128, 0, 0, 128}));
#endif
   }

   {
      // Nor is a shadow of a fully transparent color.
      image img{100, 100, 1};
      render(img, [](canvas& cnv)
      {
         cnv.shadow_style({30, 0}, 10, rgba(0, 0, 0, 0));
         cnv.fill_rect(10, 10, 20, 20);
      });
#if defined(ARTIST_RECORDING)
      CHECK(same_rect(recorded(img, 0).bounds, recorded(img, 0).geometry));
#else
      CHECK(pixel_at(img, 50, 20).a == 0);
#endif
   }
}

TEST_CASE("canvas styles: shadow ignores the transform", "[styles]")
{
   {
      // After scale(2), an offset of 30 is still 30.
      image img{100, 100, 1};
      render(img, [](canvas& cnv)
      {
         cnv.scale(2);
         cnv.shadow_style({30, 0}, 0, blue);
         cnv.fill_rect(5, 5, 10, 10);
      });
#if defined(ARTIST_RECORDING)
      CHECK(same_rect(recorded(img, 0).geometry, {10, 10, 30, 30}));
      CHECK(recorded(img, 0).bounds.right == Approx(60));
#else
      CHECK(near(pixel_at(img, 50, 20), blue8));
      CHECK(empty(img, 80, 20));
#endif
   }

   {
      // After a quarter turn, it still points along x.
      image img{100, 100, 1};
      render(img, [](canvas& cnv)
      {
         cnv.translate(50, 30);
         cnv.rotate(std::numbers::pi_v<float> / 2);
         cnv.shadow_style({30, 0}, 0, blue);
         cnv.fill_rect(-10, -10, 20, 20);
      });
#if defined(ARTIST_RECORDING)
      CHECK(same_rect(recorded(img, 0).geometry, {40, 20, 60, 40}));
      CHECK(recorded(img, 0).bounds.right == Approx(90));
      CHECK(recorded(img, 0).bounds.bottom == Approx(40));
#else
      CHECK(near(pixel_at(img, 80, 30), blue8));
      CHECK(empty(img, 50, 60));
#endif
   }

   {
      // On an image at scale 2 it is in the image's units, not pixels.
      image img{100, 100, 2};
      render(img, [](canvas& cnv)
      {
         cnv.shadow_style({30, 0}, 0, blue);
         cnv.fill_rect(10, 10, 20, 20);
      });
#if defined(ARTIST_RECORDING)
      // The journal is in the image's units.
      CHECK(same_rect(recorded(img, 0).geometry, {10, 10, 30, 30}));
      CHECK(recorded(img, 0).bounds.right == Approx(60));
#else
      CHECK(near(pixel_at(img, 100, 40), blue8));
      CHECK(empty(img, 140, 40));
#endif
   }
}

TEST_CASE("canvas styles: shadow blur spread", "[styles]")
{
   // blur is twice the Gaussian standard deviation. A straight edge blurred
   // with sigma s is at 16% at a distance of s, so with blur 20 the shadow
   // 10.5 units past the edge is about 37 of 255. A sigma of 20 would put
   // it near 76. The spread ignores the transform and the image scale.
   auto spread = [](image& img, float scale)
   {
      render(img, [scale](canvas& cnv)
      {
         cnv.scale(scale);
         cnv.shadow_style({0, 0}, 20, colors::black);
         cnv.fill_rect(-100 / scale, -100 / scale, 140 / scale, 300 / scale);
      });
   };

   for (float scale : {1.0f, 2.0f})
   {
      image img{100, 100, 1};
      spread(img, scale);
#if defined(ARTIST_RECORDING)
      // The falloff is rasterization; the journal holds the blur and its
      // reach, in surface units whatever the transform.
      CHECK(recorded(img, 0).shadow_blur == 20);
      CHECK(recorded(img, 0).bounds.right == Approx(60));
#else
      auto a = pixel_at(img, 50, 50).a;
      INFO("scale " << scale << ", alpha 10.5 past the edge: " << a);
      CHECK(a > 20);
      CHECK(a < 55);
#endif
   }

   {
      image img{100, 100, 2};
      spread(img, 1);
#if defined(ARTIST_RECORDING)
      CHECK(recorded(img, 0).shadow_blur == 20);
      CHECK(recorded(img, 0).bounds.right == Approx(60));
#else
      auto a = pixel_at(img, 101, 100).a;
      INFO("image scale 2, alpha 10.75 past the edge: " << a);
      CHECK(a > 20);
      CHECK(a < 55);
#endif
   }
}

TEST_CASE("canvas styles: Compositing", "[styles]")
{
   {
      // copy replaces what is under the shape, alpha included.
      image img{100, 100, 1};
      render(img, [](canvas& cnv)
      {
         cnv.fill_style(red);
         cnv.fill_rect(0, 0, 100, 100);
         cnv.composite_op(canvas::copy);
         cnv.fill_style(rgba(0, 0, 255, 128));
         cnv.fill_rect(40, 40, 20, 20);
      });
#if defined(ARTIST_RECORDING)
      REQUIRE(recorded(img).size() == 2);
      CHECK(recorded(img, 1).composite == canvas::copy);
      CHECK(same_color(recorded(img, 1).paint, rgba(0, 0, 255, 128)));
#else
      CHECK(near(pixel_at(img, 50, 50), {0, 0, 128, 128}));
#endif
   }

   {
      // global_composite_operation is the same call.
      image img{100, 100, 1};
      render(img, [](canvas& cnv)
      {
         cnv.fill_style(red);
         cnv.fill_rect(0, 0, 100, 100);
         cnv.global_composite_operation(canvas::destination_over);
         cnv.fill_style(blue);
         cnv.fill_rect(40, 40, 20, 20);
      });
#if defined(ARTIST_RECORDING)
      REQUIRE(recorded(img).size() == 2);
      CHECK(recorded(img, 1).composite == canvas::destination_over);
#else
      CHECK(near(pixel_at(img, 50, 50), red8));
#endif
   }

   {
      // lighter adds.
      image img{100, 100, 1};
      render(img, [](canvas& cnv)
      {
         cnv.fill_style(rgba(100, 0, 0, 255));
         cnv.fill_rect(0, 0, 100, 100);
         cnv.composite_op(canvas::lighter);
         cnv.fill_rect(40, 40, 20, 20);
      });
#if defined(ARTIST_RECORDING)
      REQUIRE(recorded(img).size() == 2);
      CHECK(recorded(img, 1).composite == canvas::lighter);
#else
      CHECK(near(pixel_at(img, 50, 50), {200, 0, 0, 255}, 6));
#endif
   }

   {
      // darker takes the smaller of each channel: 70% gray over 60% gray
      // stays 60%.
      image img{100, 100, 1};
      render(img, [](canvas& cnv)
      {
         cnv.fill_style(rgba(153, 153, 153, 255));
         cnv.fill_rect(0, 0, 100, 100);
         cnv.composite_op(canvas::darker);
         cnv.fill_style(rgba(178, 178, 178, 255));
         cnv.fill_rect(40, 40, 20, 20);
      });
#if defined(ARTIST_RECORDING)
      REQUIRE(recorded(img).size() == 2);
      CHECK(recorded(img, 1).composite == canvas::darker);
#else
      INFO("darker: " << pixel_at(img, 50, 50).r);
      CHECK(near(pixel_at(img, 50, 50), {153, 153, 153, 255}, 6));
#endif
   }
}

TEST_CASE("canvas styles: blend modes inside a clip", "[styles]")
{
   // A blend mode works inside a clip as it does outside one: 50% gray
   // multiplied by 50% gray is 25%.
   image img{100, 100, 1};
   render(img, [](canvas& cnv)
   {
      cnv.add_rect(20, 20, 60, 60);
      cnv.clip();
      cnv.fill_style(rgba(128, 128, 128, 255));
      cnv.fill_rect(0, 0, 100, 100);
      cnv.composite_op(canvas::multiply);
      cnv.fill_rect(40, 40, 20, 20);
   });
#if defined(ARTIST_RECORDING)
   REQUIRE(recorded(img).size() == 2);
   CHECK(recorded(img, 1).composite == canvas::multiply);
   CHECK(same_rect(recorded(img, 1).bounds, {40, 40, 60, 60}));
#else
   INFO("multiply inside a clip: " << pixel_at(img, 50, 50).r);
   CHECK(near(pixel_at(img, 50, 50), {64, 64, 64, 255}, 6));
   CHECK(near(pixel_at(img, 30, 30), {128, 128, 128, 255}));
#endif
}

TEST_CASE("canvas styles: unbounded operators", "[styles]")
{
   // source_in, source_out, destination_in, destination_atop and copy clear
   // the destination everywhere outside the shape, as far as the clip.
   struct probe
   {
      canvas::composite_op_enum  op;
      char const*                name;
      rgba8                      inside;     // at (50, 20), in the shape
   };

   probe const probes[] = {
      {canvas::source_in,         "source_in",          blue8},
      {canvas::source_out,        "source_out",         {0, 0, 0, 0}},
      {canvas::destination_in,    "destination_in",     red8},
      {canvas::destination_atop,  "destination_atop",   red8},
      {canvas::copy,              "copy",               blue8}
   };

   image pic{20, 20, 1};
   render(pic, [](canvas& cnv)
   {
      cnv.fill_style(blue);
      cnv.fill_rect(0, 0, 20, 20);
   });

   for (auto const& pr : probes)
   {
      // With a fill, and with an image.
      for (bool with_image : {false, true})
      {
         image img{100, 100, 1};
         render(img, [&](canvas& cnv)
         {
            cnv.fill_style(red);
            cnv.fill_rect(0, 0, 100, 100);
            cnv.add_rect(0, 0, 100, 50);
            cnv.clip();
            cnv.composite_op(pr.op);
            if (with_image)
            {
               cnv.draw(pic, point{40, 10});
            }
            else
            {
               cnv.fill_style(blue);
               cnv.fill_rect(40, 10, 20, 20);
            }
         });
         INFO(pr.name << (with_image? " image" : " fill"));
#if defined(ARTIST_RECORDING)
         // What is drawn reaches the whole clip, and no further.
         auto const& last = recorded(img).commands().back();
         CHECK(last.composite == pr.op);
         CHECK(same_rect(last.bounds, {0, 0, 100, 50}));
#else
         CHECK(pixel_at(img, 10, 10).a == 0);         // outside the shape
         CHECK(near(pixel_at(img, 10, 80), red8));    // outside the clip
         CHECK(near(pixel_at(img, 50, 20), pr.inside));
#endif
      }
   }
}

namespace
{
   color const ghost = rgba(176, 176, 176, 255);   // #b0b0b0
   color const accent = rgba(21, 101, 192, 255);   // #1565c0
   color const amber = rgba(255, 179, 0, 255);     // #ffb300
   color const green = rgba(67, 160, 71, 255);     // #43a047
   color const scarlet = rgba(229, 57, 53, 255);   // #e53935

   void caption(canvas& cnv, char const* s, float x, float y)
   {
      cnv.font(font_descr{"Open Sans", 15});
      cnv.fill_style(colors::black);
      cnv.text_align(canvas::center | canvas::baseline);
      cnv.fill_text(s, x, y);
   }

   // The path itself, drawn thin and white over its stroke.
   void centreline(canvas& cnv, std::initializer_list<point> pts)
   {
      cnv.line_width(1.25);
      cnv.line_cap(canvas::butt);
      cnv.line_join(canvas::miter_join);
      cnv.stroke_style(colors::white);
      bool first = true;
      for (auto p : pts)
      {
         if (first)
            cnv.move_to(p);
         else
            cnv.line_to(p);
         first = false;
      }
      cnv.stroke();
   }

   // A white figure of the page's width, rendered by f and saved as name.
   template <typename F>
   void figure(float h, char const* name, F f)
   {
      float const w = 560;
      image img{w, h, 2};
      {
         offscreen_image ctx{img};
         canvas cnv{ctx.context()};
         cnv.fill_style(colors::white);
         cnv.fill_rect(0, 0, w, h);
         f(cnv);
      }
      img.save_png(get_results_path() + name);
   }

   // A light checkerboard, the usual sign for transparency.
   void checkerboard(canvas& cnv, rect r, float step = 10)
   {
      auto s = cnv.new_state();
      cnv.add_rect(r);
      cnv.clip();
      cnv.fill_style(rgba(232, 232, 232, 255));
      for (float y = r.top; y < r.bottom; y += step)
      {
         for (float x = r.left; x < r.right; x += step)
         {
            if (int((x - r.left) / step + (y - r.top) / step) % 2)
               cnv.add_rect(x, y, step, step);
         }
      }
      cnv.fill();
   }
}

TEST_CASE("canvas styles: paint figure", "[styles]")
{
   // The page figure images/canvas/paint.png.
   figure(240, "canvas_paint.png", [](canvas& cnv)
   {
      char const* names[] = {
         "fill_style(color)", "fill_style(gradient)",
         "stroke_style(color)", "stroke_style(gradient)"
      };

      for (int i = 0; i != 4; ++i)
      {
         auto x = 50.0f + (i % 2) * 280;
         auto y = 20.0f + (i / 2) * 120;
         auto r = rect{x, y, extent{180, 64}};

         canvas::linear_gradient gr{r.left, 0, r.right, 0};
         gr.add_color_stop(0, accent);
         gr.add_color_stop(1, amber);

         if (i < 2)
         {
            if (i == 0)
               cnv.fill_style(accent);
            else
               cnv.fill_style(gr);
            cnv.fill_round_rect(r, 12);
         }
         else
         {
            if (i == 2)
               cnv.stroke_style(accent);
            else
               cnv.stroke_style(gr);
            cnv.line_width(12);
            cnv.stroke_round_rect(r.inset(6, 6), 6);
         }
         caption(cnv, names[i], x + 90, y + 88);
      }
   });
}

TEST_CASE("canvas styles: line_width figure", "[styles]")
{
   // The page figure images/canvas/line_width.png: three widths, each
   // centred on the same path.
   figure(125, "canvas_line_width.png", [](canvas& cnv)
   {
      char const* names[] = {
         "line_width(2)", "line_width(10)", "line_width(30)"
      };
      float const widths[] = {2, 10, 30};

      for (int i = 0; i != 3; ++i)
      {
         auto cx = 100.0f + i * 180;
         float const y = 50;

         cnv.stroke_style(accent);
         cnv.line_width(widths[i]);
         hline(cnv, cx - 60, cx + 60, y);

         if (widths[i] > 5)
            centreline(cnv, {{cx - 60, y}, {cx + 60, y}});
         caption(cnv, names[i], cx, 110);
      }
   });
}

TEST_CASE("canvas styles: line_cap figure", "[styles]")
{
   // The page figure images/canvas/line_cap.png: one line under each cap,
   // with guides at its true endpoints.
   figure(145, "canvas_line_cap.png", [](canvas& cnv)
   {
      char const* names[] = {
         "canvas::butt", "canvas::round", "canvas::square"
      };
      canvas::line_cap_enum caps[] = {
         canvas::butt, canvas::round, canvas::square
      };

      for (int i = 0; i != 3; ++i)
      {
         auto cx = 100.0f + i * 180;
         auto x1 = cx - 45, x2 = cx + 45;
         float const y = 60;

         cnv.stroke_style(ghost);
         cnv.line_width(1.25);
         for (auto x : {x1, x2})
         {
            cnv.move_to(x, y - 38);
            cnv.line_to(x, y + 38);
         }
         cnv.stroke();

         cnv.stroke_style(accent);
         cnv.line_width(30);
         cnv.line_cap(caps[i]);
         hline(cnv, x1, x2, y);

         centreline(cnv, {{x1, y}, {x2, y}});
         caption(cnv, names[i], cx, 130);
      }
   });
}

TEST_CASE("canvas styles: line_join figure", "[styles]")
{
   // The page figure images/canvas/line_join.png: one corner under each
   // join.
   figure(165, "canvas_line_join.png", [](canvas& cnv)
   {
      char const* names[] = {
         "canvas::bevel_join", "canvas::round_join", "canvas::miter_join"
      };
      canvas::join_enum joins[] = {
         canvas::bevel_join, canvas::round_join, canvas::miter_join
      };

      for (int i = 0; i != 3; ++i)
      {
         auto cx = 100.0f + i * 180;
         point const pts[] = {{cx - 45, 115}, {cx, 45}, {cx + 45, 115}};

         cnv.stroke_style(accent);
         cnv.line_width(26);
         cnv.line_join(joins[i]);
         cnv.move_to(pts[0]);
         cnv.line_to(pts[1]);
         cnv.line_to(pts[2]);
         cnv.stroke();

         centreline(cnv, {pts[0], pts[1], pts[2]});
         caption(cnv, names[i], cx, 152);
      }
   });
}

TEST_CASE("canvas styles: miter_limit figure", "[styles]")
{
   // The page figure images/canvas/miter_limit.png: the same sharp corner
   // under two limits.
   figure(200, "canvas_miter_limit.png", [](canvas& cnv)
   {
      char const* names[] = {"miter_limit(10)", "miter_limit(2)"};
      float const limits[] = {10, 2};
      auto const a = 12 * std::numbers::pi_v<float> / 180;

      for (int i = 0; i != 2; ++i)
      {
         auto cx = 140.0f + i * 280;
         point const pts[] = {
            {cx - 110 * std::sin(a), 55 + 110 * std::cos(a)},
            {cx, 55},
            {cx + 110 * std::sin(a), 55 + 110 * std::cos(a)}
         };

         cnv.stroke_style(accent);
         cnv.line_width(12);
         cnv.line_join(canvas::miter_join);
         cnv.miter_limit(limits[i]);
         cnv.move_to(pts[0]);
         cnv.line_to(pts[1]);
         cnv.line_to(pts[2]);
         cnv.stroke();

         centreline(cnv, {pts[0], pts[1], pts[2]});
         caption(cnv, names[i], cx, 190);
      }
   });
}

TEST_CASE("canvas styles: shadow figure", "[styles]")
{
   // The page figure images/canvas/shadow.png.
   figure(260, "canvas_shadow.png", [](canvas& cnv)
   {
      char const* names[] = {
         "offset {8, 8}, blur 0", "offset {8, 8}, blur 12",
         "no offset, blur 16", "a half-transparent shape"
      };

      for (int i = 0; i != 4; ++i)
      {
         auto x = 70.0f + (i % 2) * 280;
         auto y = 22.0f + (i / 2) * 125;
         auto r = rect{x, y, extent{140, 64}};
         auto shade = colors::black.opacity(0.45);

         auto s = cnv.new_state();
         switch (i)
         {
            case 0: cnv.shadow_style({8, 8}, 0, shade); break;
            case 1: cnv.shadow_style({8, 8}, 12, shade); break;
            case 2: cnv.shadow_style(16, shade); break;
            case 3: cnv.shadow_style({8, 8}, 0, shade); break;
         }
         cnv.fill_style(i == 3? accent.opacity(0.5) : accent);
         cnv.fill_round_rect(r, 10);
         cnv.shadow_style({0, 0}, 0, colors::black);
         caption(cnv, names[i], x + 70, y + 98);
      }
   });
}

namespace
{
   // One compositing cell: a destination square, then a source circle drawn
   // with op, on a transparent image, so what an operator clears shows as
   // the checkerboard.
   void composite_cell(
      canvas& cnv, point at, canvas::composite_op_enum op, bool blend)
   {
      float const w = 120, h = 100;
      image cell{w, h, 2};
      {
         offscreen_image ctx{cell};
         canvas c{ctx.context()};

         if (blend)
         {
            canvas::linear_gradient gr{12, 0, 72, 0};
            gr.add_color_stop(0, accent);
            gr.add_color_stop(1, green);
            c.fill_style(gr);
         }
         else
         {
            c.fill_style(accent);
         }
         c.fill_rect(12, 8, 60, 60);

         c.composite_op(op);
         if (blend)
         {
            canvas::linear_gradient gr{0, 30, 0, 94};
            gr.add_color_stop(0, amber);
            gr.add_color_stop(1, scarlet);
            c.fill_style(gr);
         }
         else
         {
            c.fill_style(amber);
         }
         c.add_circle(78, 62, 32);
         c.fill();
      }
      checkerboard(cnv, {at, extent{w, h}});
      cnv.draw(cell, at);
   }

   void composite_figure(char const* name, int first, bool blend)
   {
      static char const* const names[] = {
         "source_over", "source_atop", "source_in", "source_out",
         "destination_over", "destination_atop", "destination_in",
         "destination_out", "lighter", "darker", "copy", "xor_",
         "difference", "exclusion", "multiply", "screen",
         "color_dodge", "color_burn", "soft_light", "hard_light",
         "hue", "saturation", "color_op", "luminosity"
      };

      figure(420, name, [&](canvas& cnv)
      {
         for (int i = 0; i != 12; ++i)
         {
            auto x = float(i % 4) * 140;
            auto y = float(i / 4) * 140;
            auto op = canvas::composite_op_enum(first + i);
            composite_cell(cnv, {x + 10, y + 6}, op, blend);
            caption(cnv, names[first + i], x + 70, y + 128);
         }
      });
   }
}

TEST_CASE("canvas styles: compositing figures", "[styles]")
{
   // The page figures images/canvas/composite_ops.png and blend_modes.png.
   composite_figure("canvas_composite_ops.png", 0, false);
   composite_figure("canvas_blend_modes.png", 12, true);
}
