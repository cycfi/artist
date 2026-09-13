/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]

   Asserts the claims of docs/modules/ROOT/pages/canvas/styles.adoc. Each
   case names the page section it comes from.

   Every probe renders onto a transparent 100 by 100 image at scale 1 and
   samples pixels, so a pixel (x, y) covers [x, x+1) by [y, y+1). Sample
   points sit at least a pixel and a half from any edge, clear of the
   anti-aliasing, and "painted" and "empty" are alpha above 200 and below
   30.
=============================================================================*/
#include "test_support.hpp"
#include <cmath>

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
      CHECK(near(pixel_at(img, 20, 20), {0, 0, 0, 255}));
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
      CHECK(near(pixel_at(img, 50, 50), red8));
      CHECK(near(pixel_at(img, 20, 50), blue8));
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
      CHECK(near(pixel_at(img, 50, 50), red8));
      CHECK(near(pixel_at(img, 20, 50), blue8));
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
      CHECK(near(pixel_at(img, 25, 50), red8));
      auto p = pixel_at(img, 75, 50);
      CHECK((p.b > 200 && p.r < 30 && p.a > 200));
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
   CHECK(near(pixel_at(img, 20, 20), red8));    // red, and not cleared
   CHECK(empty(img, 20, 70));                   // no shadow
   CHECK(pixel_at(img, 70, 49).r > 100);        // red stroke
   CHECK(empty(img, 70, 45));                   // 2 wide, not 40
   CHECK(empty(img, 46, 50));                   // butt, not square
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
      CHECK(painted(img, 50, 46));
      CHECK(painted(img, 50, 53));
      CHECK(empty(img, 50, 43));
      CHECK(empty(img, 50, 56));
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
      CHECK(painted(img, 50, 42));
      CHECK(painted(img, 50, 57));
      CHECK(empty(img, 50, 37));
      CHECK(empty(img, 50, 62));
   }
}

TEST_CASE("canvas styles: default line width current behaviour", "[styles]")
{
   // BEHAVIOUR UNDER REVIEW. The W3C default is 1. Cairo keeps cairo's own
   // default of 2, and Skia's paint defaults to 0, a hairline. A 1 unit line
   // on y = 50 covers half of rows 49 and 50; a 2 unit line covers both.
   image img{100, 100, 1};
   render(img, [](canvas& cnv) { hline(cnv, 10, 90, 50); });
#if defined(ARTIST_CAIRO)
   CHECK(painted(img, 50, 49));
   CHECK(painted(img, 50, 50));
#elif defined(ARTIST_QUARTZ_2D)
   CHECK(std::abs(pixel_at(img, 50, 49).a - 128) < 20);
   CHECK(std::abs(pixel_at(img, 50, 50).a - 128) < 20);
#endif
   CHECK(empty(img, 50, 47));
}

TEST_CASE("canvas styles: zero line width current behaviour", "[styles]")
{
   // BEHAVIOUR UNDER REVIEW. The W3C API ignores a width of zero and keeps
   // the width it had. Cairo and Quartz 2D stroke nothing; Skia draws a
   // hairline.
   image img{100, 100, 1};
   render(img, [](canvas& cnv)
   {
      cnv.line_width(0);
      hline(cnv, 10, 90, 50.5);
   });
   int most = 0;
   for (int y = 45; y != 56; ++y)
      most = std::max(most, pixel_at(img, 50, y).a);
   INFO("most alpha near the line: " << most);
#if defined(ARTIST_CAIRO) || defined(ARTIST_QUARTZ_2D)
   CHECK(most == 0);
#endif
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
      CHECK(painted(img, 32, 50));
      CHECK(empty(img, 27, 50));
   }

   {
      image img{100, 100, 1};
      capped(img, canvas::round);
      CHECK(painted(img, 22, 50));     // 7.5 from the endpoint
      CHECK(empty(img, 21, 41));       // 12 from it, past the half circle
   }

   {
      image img{100, 100, 1};
      capped(img, canvas::square);
      CHECK(painted(img, 22, 50));
      CHECK(painted(img, 21, 41));     // the square's corner
      CHECK(empty(img, 17, 50));
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
      CHECK(painted(img, 76, 33));
      CHECK(painted(img, 73, 37));
   }

   {
      image img{100, 100, 1};
      joined(img, canvas::bevel_join);
      CHECK(empty(img, 76, 33));
      CHECK(empty(img, 73, 37));
   }

   {
      image img{100, 100, 1};
      joined(img, canvas::round_join);
      CHECK(empty(img, 76, 33));
      CHECK(painted(img, 73, 37));
   }
}

TEST_CASE("canvas styles: miter_limit", "[styles]")
{
   // A 10 wide V turning at (50, 30) through 30 degrees. Its miter is
   // 1 / sin(15 degrees) = 3.86 line widths long, so the tip reaches
   // y = 10.7 under a limit of 10, and the join is cut back to a bevel
   // under 2.
   auto vee = [](image& img, int limit)
   {
      render(img, [limit](canvas& cnv)
      {
         auto const a = 15 * 3.14159265f / 180;
         cnv.line_width(10);
         if (limit > 0)
            cnv.miter_limit(float(limit));
         else if (limit == 0)
         {
            cnv.miter_limit(2);
            cnv.miter_limit();    // back to 10
         }
         cnv.move_to(50 - 70 * std::sin(a), 30 + 70 * std::cos(a));
         cnv.line_to(50, 30);
         cnv.line_to(50 + 70 * std::sin(a), 30 + 70 * std::cos(a));
         cnv.stroke();
      });
   };

   // Default (-1), the no-argument reset (0), and an explicit 10.
   for (int limit : {-1, 0, 10})
   {
      image img{100, 100, 1};
      vee(img, limit);
      CHECK(painted(img, 50, 20));
   }

   {
      image img{100, 100, 1};
      vee(img, 2);
      CHECK(empty(img, 50, 20));
      CHECK(painted(img, 50, 40));
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
      CHECK(near(pixel_at(img, 20, 20), red8));
      CHECK(near(pixel_at(img, 50, 20), blue8));
      CHECK(empty(img, 70, 20));
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
      CHECK(near(pixel_at(img, 50, 20), blue8));
   }

   {
      // The shadow color's alpha is honoured.
      image img{100, 100, 1};
      render(img, [](canvas& cnv)
      {
         cnv.shadow_style({30, 0}, 0, rgba(0, 0, 0, 128));
         cnv.fill_rect(10, 10, 20, 20);
      });
      CHECK(near(pixel_at(img, 50, 20), {0, 0, 0, 128}));
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
      CHECK(near(pixel_at(img, 50, 50), blue8));
   }

   {
      // The offset is not scaled by the transform: after scale(2), 30 is
      // still 30 device units.
      image img{100, 100, 1};
      render(img, [](canvas& cnv)
      {
         cnv.scale(2);
         cnv.shadow_style({30, 0}, 0, blue);
         cnv.fill_rect(5, 5, 10, 10);
      });
      CHECK(near(pixel_at(img, 50, 20), blue8));
      CHECK(empty(img, 80, 20));
   }

   {
      // With no offset, a blurred shadow is a glow around the shape.
      image img{100, 100, 1};
      render(img, [](canvas& cnv)
      {
         cnv.shadow_style(8, rgba(0, 0, 0, 255));
         cnv.fill_rect(40, 40, 20, 20);
      });
      auto a = pixel_at(img, 36, 50).a;
      CHECK(a > 10);
      CHECK(a < 250);
   }
}

TEST_CASE("canvas styles: shadow blur spread", "[styles]")
{
   // blur is twice the Gaussian standard deviation, as in the W3C API. A
   // straight edge blurred with sigma s is at 16% at a distance of s, so
   // with blur 20 the shadow 10.5 units past the edge is about 37 of 255.
   // A sigma of 20 would put it near 76.
   image img{100, 100, 1};
   render(img, [](canvas& cnv)
   {
      cnv.shadow_style({0, 0}, 20, rgba(0, 0, 0, 255));
      cnv.fill_rect(-100, -100, 140, 300);
   });
   auto a = pixel_at(img, 50, 50).a;
   INFO("alpha 10.5 past the edge: " << a);
   CHECK(a > 20);
   CHECK(a < 55);
}

TEST_CASE("canvas styles: shadow of a transparent shape current behaviour",
   "[styles]")
{
   // BEHAVIOUR UNDER REVIEW. In the W3C API the shadow is cast by what is
   // drawn, so a fully transparent fill casts none. Cairo draws the path in
   // the shadow color whatever the fill, so it casts one anyway.
   image img{100, 100, 1};
   render(img, [](canvas& cnv)
   {
      cnv.shadow_style({30, 0}, 0, blue);
      cnv.fill_style(rgba(255, 0, 0, 0));
      cnv.fill_rect(10, 10, 20, 20);
   });
#if defined(ARTIST_CAIRO)
   CHECK(near(pixel_at(img, 50, 20), blue8));
#elif defined(ARTIST_QUARTZ_2D)
   CHECK(empty(img, 50, 20));
#endif
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
      CHECK(near(pixel_at(img, 50, 50), {0, 0, 128, 128}));
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
      CHECK(near(pixel_at(img, 50, 50), red8));
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
      CHECK(near(pixel_at(img, 50, 50), {200, 0, 0, 255}, 6));
   }
}

TEST_CASE("canvas styles: darker current behaviour", "[styles]")
{
   // BEHAVIOUR UNDER REVIEW. Over opaque 60% gray, 70% gray drawn with
   // darker is max(0, 0.6 + 0.7 - 1) = 30% under the plus-darker rule, which
   // Quartz 2D implements. Cairo, Skia and Direct2D take the per-channel
   // minimum, 60%.
   image img{100, 100, 1};
   render(img, [](canvas& cnv)
   {
      cnv.fill_style(rgba(153, 153, 153, 255));
      cnv.fill_rect(0, 0, 100, 100);
      cnv.composite_op(canvas::darker);
      cnv.fill_style(rgba(178, 178, 178, 255));
      cnv.fill_rect(40, 40, 20, 20);
   });
   auto p = pixel_at(img, 50, 50);
   INFO("darker: " << p.r);
#if defined(ARTIST_CAIRO)
   CHECK(std::abs(p.r - 153) <= 6);
#elif defined(ARTIST_QUARTZ_2D)
   CHECK(std::abs(p.r - 77) <= 6);
#endif
}

TEST_CASE("canvas styles: unbounded operators current behaviour", "[styles]")
{
   // BEHAVIOUR UNDER REVIEW. In the W3C API these five also clear everything
   // outside the shape. Report, for each, what is left at a corner far from
   // the shape.
   canvas::composite_op_enum const ops[] = {
      canvas::source_in, canvas::source_out,
      canvas::destination_in, canvas::destination_atop, canvas::copy
   };
   char const* names[] = {
      "source_in", "source_out", "destination_in", "destination_atop", "copy"
   };

   for (int i = 0; i != 5; ++i)
   {
      image img{100, 100, 1};
      render(img, [op = ops[i]](canvas& cnv)
      {
         cnv.fill_style(red);
         cnv.fill_rect(0, 0, 100, 100);
         cnv.composite_op(op);
         cnv.fill_style(blue);
         cnv.fill_rect(40, 40, 20, 20);
      });
      auto corner = pixel_at(img, 10, 10);
      INFO(names[i] << " corner alpha: " << corner.a);
#if defined(ARTIST_CAIRO)
      if (ops[i] == canvas::copy)
         CHECK(near(corner, red8));      // cairo's SOURCE is bounded
      else
         CHECK(corner.a == 0);
#elif defined(ARTIST_QUARTZ_2D)
      CHECK(near(corner, red8));
#endif
   }
}

namespace
{
   color const ghost = rgba(176, 176, 176, 255);   // #b0b0b0
   color const accent = rgba(21, 101, 192, 255);   // #1565c0

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
}

TEST_CASE("canvas styles: line_cap figure", "[styles]")
{
   // The page figure images/canvas/line_cap.png: one line under each cap,
   // with guides at its true endpoints.
   float const w = 560, h = 145;
   image img{w, h, 2};
   {
      offscreen_image ctx{img};
      canvas cnv{ctx.context()};
      cnv.fill_style(colors::white);
      cnv.fill_rect(0, 0, w, h);

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
   }
   img.save_png(get_results_path() + "canvas_line_cap.png");
}

TEST_CASE("canvas styles: line_join figure", "[styles]")
{
   // The page figure images/canvas/line_join.png: one corner under each
   // join.
   float const w = 560, h = 165;
   image img{w, h, 2};
   {
      offscreen_image ctx{img};
      canvas cnv{ctx.context()};
      cnv.fill_style(colors::white);
      cnv.fill_rect(0, 0, w, h);

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
   }
   img.save_png(get_results_path() + "canvas_line_join.png");
}
