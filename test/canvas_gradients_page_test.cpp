/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]

   Asserts the claims of docs/modules/ROOT/pages/canvas/gradients.adoc and
   draws its figures. Each case names the page section it comes from.

   Every probe renders onto a transparent 100 by 100 image and samples
   pixels, so a pixel (x, y) covers [x, x+1) by [y, y+1). Sample points sit
   clear of a stop, where the gradient is flat or nearly so.

   Stop colors are compared by hue rather than by value: Quartz 2D renders a
   gradient's stops through kCGColorSpaceGenericRGB while a flat fill goes
   through the device space, so the same color comes out shifted there (red
   255, 0, 0 renders 255, 38, 0). That difference is on the page as a
   CAUTION and awaits a ruling; see redish/blueish below.
=============================================================================*/
#include "test_support.hpp"
#include <functional>

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

   // The two figure colors, told apart by which channel dominates. This is
   // the comparison every case uses, so no assertion depends on the Quartz
   // 2D color space shift.
   bool redish(rgba8 p)   { return p.a > 200 && p.r > 200 && p.b < 60; }
   bool blueish(rgba8 p)  { return p.a > 200 && p.b > 200 && p.r < 60; }
   bool blended(rgba8 p)  { return p.a > 200 && p.r > 60 && p.r < 200 && p.b > 60; }
   bool clear(rgba8 p)    { return p.a < 30; }

   // How far along a red to blue ramp a pixel sits, 0 at red and 1 at blue.
   float ramp_at(image const& img, int x, int y)
   {
      auto p = pixel_at(img, x, y);
      return float(p.b) / float(p.r + p.b);
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

   // A red to blue linear gradient running from x1 to x2.
   canvas::linear_gradient ramp(float x1, float x2)
   {
      canvas::linear_gradient gr{x1, 0, x2, 0};
      gr.add_color_stop(0, red);
      gr.add_color_stop(1, blue);
      return gr;
   }
}

TEST_CASE("canvas gradients: Color Stops", "[gradients]")
{
   {
      // A gradient runs between its stops, and holds the end colors past
      // them: the first stop's color reaches back to the start of what is
      // painted, the last stop's color forward to the end.
      image img{100, 100, 1};
      render(img, [](canvas& cnv)
      {
         cnv.fill_style(ramp(20, 80));
         cnv.fill_rect(0, 0, 100, 100);
      });
#if defined(ARTIST_RECORDING)
      REQUIRE(recorded(img).size() == 1);
      CHECK(recorded(img, 0).gradient);
#else
      CHECK(redish(pixel_at(img, 5, 50)));      // before the start
      CHECK(blended(pixel_at(img, 50, 50)));    // between the stops
      CHECK(blueish(pixel_at(img, 95, 50)));    // past the end
      // The ramp rises from red to blue with distance.
      CHECK(ramp_at(img, 35, 50) < ramp_at(img, 50, 50));
      CHECK(ramp_at(img, 50, 50) < ramp_at(img, 65, 50));
#endif
   }

   {
      // One stop is that color everywhere, wherever the stop sits.
      image img{100, 100, 1};
      render(img, [](canvas& cnv)
      {
         canvas::linear_gradient gr{20, 0, 80, 0};
         gr.add_color_stop(0.5, red);
         cnv.fill_style(gr);
         cnv.fill_rect(0, 0, 100, 100);
      });
#if !defined(ARTIST_RECORDING)
      CHECK(redish(pixel_at(img, 10, 50)));
      CHECK(redish(pixel_at(img, 50, 50)));
      CHECK(redish(pixel_at(img, 90, 50)));
#endif
   }

   {
      // No stops paints nothing, as the W3C API specifies.
      image img{100, 100, 1};
      render(img, [](canvas& cnv)
      {
         canvas::linear_gradient gr{20, 0, 80, 0};
         cnv.fill_style(gr);
         cnv.fill_rect(0, 0, 100, 100);
      });
#if !defined(ARTIST_RECORDING)
      CHECK(clear(pixel_at(img, 50, 50)));
#endif
   }

   {
      // Stops need not be added in order of offset; they are sorted.
      image img{100, 100, 1};
      render(img, [](canvas& cnv)
      {
         canvas::linear_gradient gr{0, 0, 100, 0};
         gr.add_color_stop(1, blue);
         gr.add_color_stop(0, red);
         cnv.fill_style(gr);
         cnv.fill_rect(0, 0, 100, 100);
      });
#if !defined(ARTIST_RECORDING)
      CHECK(ramp_at(img, 5, 50) < ramp_at(img, 50, 50));
      CHECK(ramp_at(img, 50, 50) < ramp_at(img, 95, 50));
#endif
   }

   {
      // Two stops at the same offset make a hard edge, with no blend
      // between them.
      image img{100, 100, 1};
      render(img, [](canvas& cnv)
      {
         canvas::linear_gradient gr{0, 0, 100, 0};
         gr.add_color_stop(0, red);
         gr.add_color_stop(0.5, red);
         gr.add_color_stop(0.5, blue);
         gr.add_color_stop(1, blue);
         cnv.fill_style(gr);
         cnv.fill_rect(0, 0, 100, 100);
      });
#if !defined(ARTIST_RECORDING)
      CHECK(redish(pixel_at(img, 25, 50)));
      CHECK(redish(pixel_at(img, 48, 50)));
      CHECK(blueish(pixel_at(img, 52, 50)));
      CHECK(blueish(pixel_at(img, 75, 50)));
#endif
   }

   {
      // add_color_stop(cs) and add_color_stop(offset, c) are the same call.
      canvas::linear_gradient a{0, 0, 100, 0};
      a.add_color_stop({0.25f, red});
      canvas::linear_gradient b{0, 0, 100, 0};
      b.add_color_stop(0.25f, red);
      REQUIRE(a.color_space.size() == 1);
      REQUIRE(b.color_space.size() == 1);
      CHECK(a.color_space[0].offset == b.color_space[0].offset);
      CHECK(a.color_space[0].color == b.color_space[0].color);
   }

   {
      // color_space holds the stops in the order they were added.
      canvas::linear_gradient gr{0, 0, 100, 0};
      gr.add_color_stop(1, blue);
      gr.add_color_stop(0, red);
      REQUIRE(gr.color_space.size() == 2);
      CHECK(gr.color_space[0].offset == 1);
      CHECK(gr.color_space[1].offset == 0);
   }
}

TEST_CASE("canvas gradients: Linear Gradients", "[gradients]")
{
   {
      // The two constructors are the same gradient.
      canvas::linear_gradient a{10, 20, 30, 40};
      canvas::linear_gradient b{point{10, 20}, point{30, 40}};
      CHECK(a.start == b.start);
      CHECK(a.end == b.end);
      CHECK(a.start == point{10, 20});
      CHECK(a.end == point{30, 40});
   }

   {
      // The ramp runs along the line from start to end, so a vertical line
      // gives a vertical gradient and the color is constant across it.
      image img{100, 100, 1};
      render(img, [](canvas& cnv)
      {
         canvas::linear_gradient gr{0, 20, 0, 80};
         gr.add_color_stop(0, red);
         gr.add_color_stop(1, blue);
         cnv.fill_style(gr);
         cnv.fill_rect(0, 0, 100, 100);
      });
#if !defined(ARTIST_RECORDING)
      CHECK(redish(pixel_at(img, 50, 5)));
      CHECK(blueish(pixel_at(img, 50, 95)));
      // Constant across the line: the same at three x for one y.
      CHECK(ramp_at(img, 10, 50) == Approx(ramp_at(img, 50, 50)).margin(0.02));
      CHECK(ramp_at(img, 90, 50) == Approx(ramp_at(img, 50, 50)).margin(0.02));
#endif
   }
}

TEST_CASE("canvas gradients: Radial Gradients", "[gradients]")
{
   {
      // The two constructors are the same gradient.
      canvas::radial_gradient a{10, 20, 5, 30, 40, 15};
      canvas::radial_gradient b{point{10, 20}, 5, point{30, 40}, 15};
      CHECK(a.c1 == b.c1);
      CHECK(a.c1_radius == b.c1_radius);
      CHECK(a.c2 == b.c2);
      CHECK(a.c2_radius == b.c2_radius);
      CHECK(a.c1 == point{10, 20});
      CHECK(a.c1_radius == 5);
      CHECK(a.c2 == point{30, 40});
      CHECK(a.c2_radius == 15);
   }

   {
      // Concentric circles, the inner one a point: the ramp runs outward
      // from the centre and holds the last stop's color past the rim.
      image img{100, 100, 1};
      render(img, [](canvas& cnv)
      {
         canvas::radial_gradient gr{point{50, 50}, 0, point{50, 50}, 40};
         gr.add_color_stop(0, red);
         gr.add_color_stop(1, blue);
         cnv.fill_style(gr);
         cnv.fill_rect(0, 0, 100, 100);
      });
#if !defined(ARTIST_RECORDING)
      CHECK(redish(pixel_at(img, 50, 50)));
      CHECK(blueish(pixel_at(img, 95, 50)));
      CHECK(ramp_at(img, 60, 50) < ramp_at(img, 70, 50));
      CHECK(ramp_at(img, 70, 50) < ramp_at(img, 80, 50));
      // The same distance out in any direction is the same color. Pixel
      // centres sit at x + 0.5, so 70 and 29 are the pair that straddles
      // the centre evenly, not 70 and 30.
      CHECK(ramp_at(img, 70, 50) == Approx(ramp_at(img, 50, 70)).margin(0.02));
      CHECK(ramp_at(img, 70, 50) == Approx(ramp_at(img, 29, 50)).margin(0.02));
#endif
   }

   {
      // A first circle with a radius of its own: everything inside it is
      // the first stop's color, and the ramp runs in the ring between the
      // two circles.
      image img{100, 100, 1};
      render(img, [](canvas& cnv)
      {
         canvas::radial_gradient gr{point{50, 50}, 20, point{50, 50}, 40};
         gr.add_color_stop(0, red);
         gr.add_color_stop(1, blue);
         cnv.fill_style(gr);
         cnv.fill_rect(0, 0, 100, 100);
      });
#if !defined(ARTIST_RECORDING)
      CHECK(redish(pixel_at(img, 50, 50)));     // inside the first circle
      CHECK(redish(pixel_at(img, 65, 50)));     // still inside it
      CHECK(blended(pixel_at(img, 80, 50)));    // in the ring
      CHECK(blueish(pixel_at(img, 95, 50)));    // outside the second
#endif
   }

   {
      // Two identical circles paint nothing, as the W3C API specifies.
      image img{100, 100, 1};
      render(img, [](canvas& cnv)
      {
         canvas::radial_gradient gr{point{50, 50}, 30, point{50, 50}, 30};
         gr.add_color_stop(0, red);
         gr.add_color_stop(1, blue);
         cnv.fill_style(gr);
         cnv.fill_rect(0, 0, 100, 100);
      });
#if !defined(ARTIST_RECORDING)
      CHECK(clear(pixel_at(img, 20, 50)));
      CHECK(clear(pixel_at(img, 50, 50)));
      CHECK(clear(pixel_at(img, 80, 50)));
#endif
   }
}

TEST_CASE("canvas gradients: Painting with a Gradient", "[gradients]")
{
   {
      // A gradient is a stroke paint as well as a fill paint, and the two
      // are separate styles.
      image img{100, 100, 1};
      render(img, [](canvas& cnv)
      {
         cnv.fill_style(red);
         cnv.stroke_style(ramp(0, 100));
         cnv.line_width(20);
         cnv.move_to(0, 50);
         cnv.line_to(100, 50);
         cnv.stroke();
      });
#if defined(ARTIST_RECORDING)
      REQUIRE(recorded(img).size() == 1);
      CHECK(recorded(img, 0).kind == recording::op::stroke);
      CHECK(recorded(img, 0).gradient);
#else
      CHECK(ramp_at(img, 5, 50) < ramp_at(img, 50, 50));
      CHECK(ramp_at(img, 50, 50) < ramp_at(img, 95, 50));
#endif
   }

   {
      // Setting a gradient replaces the color that was there, and setting a
      // color replaces the gradient.
      image img{100, 100, 1};
      render(img, [](canvas& cnv)
      {
         cnv.fill_style(ramp(0, 100));
         cnv.fill_style(red);
         cnv.fill_rect(0, 0, 100, 100);
      });
#if !defined(ARTIST_RECORDING)
      CHECK(redish(pixel_at(img, 5, 50)));
      CHECK(redish(pixel_at(img, 95, 50)));
#endif
   }

   {
      // The gradient is copied when the style is set. Adding a stop to the
      // object afterwards does not reach the style already set.
      image img{100, 100, 1};
      render(img, [](canvas& cnv)
      {
         canvas::linear_gradient gr{0, 0, 100, 0};
         gr.add_color_stop(0, red);
         cnv.fill_style(gr);
         gr.add_color_stop(1, blue);
         cnv.fill_rect(0, 0, 100, 100);
      });
#if !defined(ARTIST_RECORDING)
      CHECK(redish(pixel_at(img, 95, 50)));
#endif
   }

   {
      // The gradient style is part of the saved state.
      image img{100, 100, 1};
      render(img, [](canvas& cnv)
      {
         cnv.fill_style(red);
         {
            auto s = cnv.new_state();
            canvas::linear_gradient gr{0, 0, 100, 0};
            gr.add_color_stop(0, blue);
            gr.add_color_stop(1, blue);
            cnv.fill_style(gr);
            cnv.fill_rect(0, 0, 100, 40);
         }
         cnv.fill_rect(0, 60, 100, 40);
      });
#if !defined(ARTIST_RECORDING)
      CHECK(blueish(pixel_at(img, 50, 20)));    // the gradient, inside
      CHECK(redish(pixel_at(img, 50, 80)));     // the color, restored
#endif
   }
}

TEST_CASE("canvas gradients: Coordinates", "[gradients]")
{
   {
      // Gradient coordinates are user space under the transform in effect
      // when the path is painted, not when the style is set: a scale after
      // fill_style stretches the ramp with everything else.
      image img{100, 100, 1};
      render(img, [](canvas& cnv)
      {
         cnv.fill_style(ramp(0, 50));
         cnv.scale(2, 2);                 // the ramp now spans 0 to 100
         cnv.fill_rect(0, 0, 50, 50);
      });
#if !defined(ARTIST_RECORDING)
      CHECK(ramp_at(img, 5, 25) < 0.15f);
      CHECK(ramp_at(img, 50, 25) == Approx(0.5f).margin(0.1));
      CHECK(ramp_at(img, 95, 25) > 0.85f);
#endif
   }

   {
      // The same for a transform that lands between two paints with one
      // gradient: the second paint follows the new transform.
      image img{100, 100, 1};
      render(img, [](canvas& cnv)
      {
         cnv.fill_style(ramp(0, 50));
         cnv.fill_rect(0, 0, 50, 40);     // device 0 to 50
         cnv.translate(50, 0);
         cnv.fill_rect(0, 60, 50, 40);    // device 50 to 100
      });
#if !defined(ARTIST_RECORDING)
      CHECK(redish(pixel_at(img, 5, 20)));      // first paint, at its start
      CHECK(blueish(pixel_at(img, 45, 20)));    // first paint, at its end
      CHECK(redish(pixel_at(img, 55, 80)));     // second paint, at its start
      CHECK(blueish(pixel_at(img, 95, 80)));    // second paint, at its end
#endif
   }
}

///////////////////////////////////////////////////////////////////////////////
// The page figures.

namespace
{
   color const ghost = rgba(176, 176, 176, 255);   // #b0b0b0
   color const ink = rgba(26, 26, 26, 255);        // #1a1a1a
   color const accent = rgba(21, 101, 192, 255);   // #1565c0
   color const amber = rgba(255, 179, 0, 255);     // #ffb300
   color const scarlet = rgba(229, 57, 53, 255);   // #e53935

   void caption(canvas& cnv, char const* s, float x, float y)
   {
      cnv.font(font_descr{"Open Sans", 15});
      cnv.fill_style(ink);
      cnv.text_align(canvas::center | canvas::baseline);
      cnv.fill_text(s, x, y);
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

   // A small marker ring at p, with its name under it.
   void marker(canvas& cnv, point p, char const* name)
   {
      auto s = cnv.new_state();
      cnv.line_width(1.5);
      cnv.stroke_style(ink);
      cnv.add_circle(p.x, p.y, 4);
      cnv.stroke();
      caption(cnv, name, p.x, p.y - 10);
   }

   // A dashed-looking guide: a thin grey line.
   void guide(canvas& cnv, point a, point b)
   {
      auto s = cnv.new_state();
      cnv.line_width(1.25);
      cnv.stroke_style(ghost);
      cnv.move_to(a);
      cnv.line_to(b);
      cnv.stroke();
   }
}

TEST_CASE("canvas gradients: linear figure", "[gradients]")
{
   // The page figure images/canvas/linear_gradient.png: one gradient with
   // its start and end marked, showing the ramp between them and the flat
   // color each side.
   figure(180, "canvas_linear_gradient.png", [](canvas& cnv)
   {
      auto box = rect{40, 45, 520, 130};
      float const x1 = 160, x2 = 400;

      canvas::linear_gradient gr{x1, 0, x2, 0};
      gr.add_color_stop(0, accent);
      gr.add_color_stop(1, amber);
      cnv.fill_style(gr);
      cnv.fill_rect(box);

      guide(cnv, {x1, box.top - 12}, {x1, box.bottom + 10});
      guide(cnv, {x2, box.top - 12}, {x2, box.bottom + 10});
      marker(cnv, {x1, box.top - 22}, "start");
      marker(cnv, {x2, box.top - 22}, "end");

      caption(cnv, "first stop held", (box.left + x1) / 2, box.bottom + 30);
      caption(cnv, "the ramp", (x1 + x2) / 2, box.bottom + 30);
      caption(cnv, "last stop held", (x2 + box.right) / 2, box.bottom + 30);
   });
}

TEST_CASE("canvas gradients: radial figure", "[gradients]")
{
   // The page figure images/canvas/radial_gradient.png: the three shapes a
   // radial gradient takes, by where the two circles sit.
   figure(230, "canvas_radial_gradient.png", [](canvas& cnv)
   {
      struct probe
      {
         char const* name;
         point       c1;
         float       c1r;
         point       c2;
         float       c2r;
      };

      probe const probes[] = {
         {"concentric, c1_radius 0", {0, 0}, 0,       {0, 0}, 70},
         {"concentric, c1_radius 30", {0, 0}, 30,     {0, 0}, 70},
         {"c1 offset from c2", {-28, -28}, 8,         {0, 0}, 70},
      };

      for (int i = 0; i != 3; ++i)
      {
         auto const& p = probes[i];
         auto o = point{100.0f + i * 180, 95};

         canvas::radial_gradient gr{
            point{o.x + p.c1.x, o.y + p.c1.y}, p.c1r,
            point{o.x + p.c2.x, o.y + p.c2.y}, p.c2r
         };
         gr.add_color_stop(0, amber);
         gr.add_color_stop(1, accent);

         auto s = cnv.new_state();
         cnv.add_circle(o.x, o.y, p.c2r);
         cnv.clip();
         cnv.fill_style(gr);
         cnv.fill_rect(o.x - p.c2r, o.y - p.c2r, 2 * p.c2r, 2 * p.c2r);
      }

      for (int i = 0; i != 3; ++i)
         caption(cnv, probes[i].name, 100.0f + i * 180, 200);
   });
}

TEST_CASE("canvas gradients: color stops figure", "[gradients]")
{
   // The page figure images/canvas/color_stops.png: what the stop list does
   // to the ramp.
   figure(330, "canvas_color_stops.png", [](canvas& cnv)
   {
      struct probe
      {
         char const*                          name;
         std::vector<canvas::color_stop>      stops;
      };

      std::vector<probe> const probes = {
         {"two stops", {{0, accent}, {1, amber}}},
         {"one stop", {{0.5f, accent}}},
         {"four stops", {{0, accent}, {0.35f, amber}, {0.7f, scarlet}, {1, accent}}},
         {"a repeated offset is a hard edge",
            {{0, accent}, {0.5f, accent}, {0.5f, amber}, {1, amber}}},
      };

      float y = 40;
      for (auto const& p : probes)
      {
         auto box = rect{40, y, 520, y + 40};
         canvas::linear_gradient gr{box.left, 0, box.right, 0};
         for (auto const& cs : p.stops)
            gr.add_color_stop(cs);
         cnv.fill_style(gr);
         cnv.fill_rect(box);
         caption(cnv, p.name, 280, y + 62);
         y += 70;
      }
   });
}

TEST_CASE("canvas gradients: Example", "[gradients]")
{
   // The page's == Example, verbatim, so it keeps compiling and running.
   image img{360, 110, 1};
   render(img, [](canvas& cnv)
   {
      auto box = rect{40, 40, 240, 92};
      canvas::linear_gradient sheen{box.left, box.top, box.left, box.bottom};
      sheen.add_color_stop(0, rgba(88, 152, 232, 255));
      sheen.add_color_stop(0.5, rgba(21, 101, 192, 255));
      sheen.add_color_stop(0.5, rgba(17, 82, 156, 255));
      sheen.add_color_stop(1, rgba(38, 118, 209, 255));
      cnv.fill_style(sheen);
      cnv.fill_round_rect(box, 8);

      auto knob = circle{300, 66, 26};
      canvas::radial_gradient lit{
         point{knob.cx - 10, knob.cy - 10}, 2,
         point{knob.cx, knob.cy}, knob.radius
      };
      lit.add_color_stop(0, rgba(150, 196, 255, 255));
      lit.add_color_stop(1, rgba(17, 82, 156, 255));
      cnv.fill_style(lit);
      cnv.add_circle(knob);
      cnv.fill();
   });

#if !defined(ARTIST_RECORDING)
   // The sheen runs light to dark down the bar, broken in the middle by
   // the pair of stops at 0.5.
   CHECK(pixel_at(img, 140, 48).b > pixel_at(img, 140, 60).b);
   CHECK(pixel_at(img, 140, 62).b < pixel_at(img, 140, 60).b);
   CHECK(pixel_at(img, 140, 84).b > pixel_at(img, 140, 68).b);

   // The knob's highlight sits up and to the left of its centre, because
   // the first circle is offset there.
   CHECK(pixel_at(img, 290, 56).b > pixel_at(img, 310, 76).b);
#endif
}
