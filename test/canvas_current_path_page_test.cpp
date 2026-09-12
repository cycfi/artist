/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]

   Asserts the claims of docs/modules/ROOT/pages/canvas/current_path.adoc.
   Each case names the page section it comes from.

   fill_extent() is the natural probe for "what is in the current path", but
   it is defined only on the Cairo backend (see the page's CAUTION), so the
   cases that use it are guarded.
=============================================================================*/
#include "test_support.hpp"
#include <numbers>
#include <vector>

namespace
{
   // Run f on a canvas over an offscreen image.
   template <typename F>
   void on_canvas(F f, float w = 100, float h = 100)
   {
      image img{w, h, 1};
      offscreen_image ctx{img};
      canvas cnv{ctx.context()};
      f(cnv);
   }

   bool near(float a, float b, float tol = 0.01f)
   {
      return std::abs(a - b) <= tol;
   }

   bool near(rect a, rect b, float tol = 0.01f)
   {
      return near(a.left, b.left, tol) && near(a.top, b.top, tol)
         && near(a.right, b.right, tol) && near(a.bottom, b.bottom, tol);
   }

   // Build a path with f, stroke it, and report which of the sample points
   // came out painted. fill_extent answers "what would a fill cover", so it
   // says nothing about a zero-area segment; this does.
   template <typename F>
   std::vector<bool> stroked(F f, std::vector<point> samples)
   {
      float const w = 100, h = 100;
      image img{w, h, 1};
      {
         offscreen_image ctx{img};
         canvas cnv{ctx.context()};
         cnv.stroke_style(colors::black);
         cnv.line_width(3);
         f(cnv);
         cnv.stroke();
      }
      auto const* px = img.pixels();
      auto bs = img.bitmap_size();
      std::vector<bool> result;
      for (auto p : samples)
      {
         auto i = int(p.y) * int(bs.x) + int(p.x);
         result.push_back((px[i] >> 24) != 0);   // any alpha at all
      }
      return result;
   }
}

#if defined(ARTIST_CAIRO)

TEST_CASE("current path: Building", "[current_path]")
{
   on_canvas([](canvas& cnv)
   {
      // An empty path has an empty fill extent.
      CHECK(cnv.fill_extent() == rect{0, 0, 0, 0});

      // add_rect puts the rectangle in the path.
      cnv.add_rect(10, 20, 30, 40);
      CHECK(near(cnv.fill_extent(), rect{10, 20, 40, 60}));

      // begin_path discards it.
      cnv.begin_path();
      CHECK(cnv.fill_extent() == rect{0, 0, 0, 0});
   });

   on_canvas([](canvas& cnv)
   {
      // Geometry accumulates: a second shape joins the first.
      cnv.add_rect(0, 0, 10, 10);
      cnv.add_rect(50, 50, 10, 10);
      CHECK(near(cnv.fill_extent(), rect{0, 0, 60, 60}));
   });

   // move_to places the current point without drawing; line_to draws to it.
   // A bare segment encloses no area, so fill_extent stays empty: it reports
   // what a fill would cover, not what geometry is in the path.
   {
      auto hit = stroked([](canvas& cnv)
      {
         cnv.move_to(20, 20);
         cnv.line_to(80, 20);
      }, {{50, 20}, {50, 60}});
      CHECK(hit[0]);
      CHECK(!hit[1]);
   }

   on_canvas([](canvas& cnv)
   {
      cnv.move_to(10, 10);
      cnv.line_to(40, 30);
      CHECK(cnv.fill_extent() == rect{0, 0, 0, 0});
   });
}

TEST_CASE("current path: Painting consumes the path", "[current_path]")
{
   on_canvas([](canvas& cnv)
   {
      cnv.fill_style(colors::red);
      cnv.add_rect(10, 10, 20, 20);
      cnv.fill();
      CHECK(cnv.fill_extent() == rect{0, 0, 0, 0});
   });

   on_canvas([](canvas& cnv)
   {
      cnv.fill_style(colors::red);
      cnv.add_rect(10, 10, 20, 20);
      cnv.fill_preserve();
      CHECK(near(cnv.fill_extent(), rect{10, 10, 30, 30}));
      cnv.stroke();
      CHECK(cnv.fill_extent() == rect{0, 0, 0, 0});
   });

   on_canvas([](canvas& cnv)
   {
      cnv.stroke_style(colors::red);
      cnv.add_rect(10, 10, 20, 20);
      cnv.stroke_preserve();
      CHECK(near(cnv.fill_extent(), rect{10, 10, 30, 30}));
   });
}

TEST_CASE("current path: add_circle joins the current point", "[current_path]")
{
   {
      // BEHAVIOUR UNDER REVIEW. On Cairo and Quartz 2D, add_circle is an arc,
      // so it is joined to the current point by a straight line: stroke the
      // pair and the joining segment is painted. Skia uses addCircle, which
      // starts a sub-path of its own and paints no such line.
      auto hit = stroked([](canvas& cnv)
      {
         cnv.move_to(10, 50);
         cnv.add_circle(70, 50, 10);
      }, {{40, 50}});
      CHECK(hit[0]);
   }

   on_canvas([](canvas& cnv)
   {
      // With no current point the circle stands alone.
      cnv.add_circle(60, 60, 10);
      CHECK(near(cnv.fill_extent(), rect{50, 50, 70, 70}));
   });
}

TEST_CASE("current path: add_round_rect clamps the radius", "[current_path]")
{
   on_canvas([](canvas& cnv)
   {
      // radius is clamped to half the shorter side, so an over-large radius
      // gives a stadium, not a bigger shape.
      cnv.add_round_rect(0, 0, 80, 40, 1000);
      CHECK(near(cnv.fill_extent(), rect{0, 0, 80, 40}));
   });
}

TEST_CASE("current path: add_path appends", "[current_path]")
{
   on_canvas([](canvas& cnv)
   {
      path p;
      p.add_rect({50, 50, 60, 60});

      cnv.add_rect(0, 0, 10, 10);
      cnv.add_path(p);
      // Cairo and Quartz 2D append. Skia replaces.
      CHECK(near(cnv.fill_extent(), rect{0, 0, 60, 60}));
   });
}

TEST_CASE("current path: arc_to degenerate cases", "[current_path]")
{
   {
      // radius 0 is a straight line to the corner.
      auto hit = stroked([](canvas& cnv)
      {
         cnv.move_to(20, 20);
         cnv.arc_to(80, 20, 80, 80, 0);
      }, {{50, 20}, {80, 50}});
      CHECK(hit[0]);
      CHECK(!hit[1]);
   }

   {
      // Collinear points have no corner to round, so it is a straight line
      // to the first point and no further.
      auto hit = stroked([](canvas& cnv)
      {
         cnv.move_to(20, 50);
         cnv.arc_to(50, 50, 80, 50, 10);
      }, {{35, 50}, {65, 50}});
      CHECK(hit[0]);
      CHECK(!hit[1]);
   }

   {
      // The rounded corner itself: the arc leaves the line from the current
      // point to p1 early, so the corner point is not painted.
      auto hit = stroked([](canvas& cnv)
      {
         cnv.move_to(20, 20);
         cnv.arc_to(80, 20, 80, 80, 30);
      }, {{40, 20}, {80, 20}});
      CHECK(hit[0]);
      CHECK(!hit[1]);
   }
}

TEST_CASE("current path: Clipping", "[current_path]")
{
   on_canvas([](canvas& cnv)
   {
      // clip() consumes the current path.
      cnv.add_rect(10, 10, 50, 50);
      cnv.clip();
      CHECK(cnv.fill_extent() == rect{0, 0, 0, 0});
      CHECK(near(cnv.clip_extent(), rect{10, 10, 60, 60}));

      // Clipping only ever narrows.
      cnv.add_rect(0, 0, 30, 30);
      cnv.clip();
      CHECK(near(cnv.clip_extent(), rect{10, 10, 30, 30}));
   });

   on_canvas([](canvas& cnv)
   {
      // save/restore puts the clip back.
      {
         auto s = cnv.new_state();
         cnv.add_rect(10, 10, 20, 20);
         cnv.clip();
         CHECK(near(cnv.clip_extent(), rect{10, 10, 30, 30}));
      }
      CHECK(near(cnv.clip_extent(), rect{0, 0, 100, 100}));
   });
}

TEST_CASE("current path: clip(path) current behaviour", "[current_path]")
{
   // BEHAVIOUR UNDER REVIEW. On Cairo, clip(p) appends p to the current path
   // and clips to the pair, then discards the path. Quartz 2D and Skia clip
   // to p alone and leave the current path untouched.
   on_canvas([](canvas& cnv)
   {
      path p;
      p.add_rect({0, 0, 40, 40});

      cnv.add_rect(50, 50, 20, 20);   // still being built
      cnv.clip(p);
      CHECK(near(cnv.clip_extent(), rect{0, 0, 70, 70}));
      CHECK(cnv.fill_extent() == rect{0, 0, 0, 0});
   });
}

TEST_CASE("current path: close_path", "[current_path]")
{
   // close_path draws the straight line back to where the sub-path began,
   // so the triangle's third side is painted.
   auto hit = stroked([](canvas& cnv)
   {
      cnv.move_to(20, 20);
      cnv.line_to(80, 20);
      cnv.line_to(80, 80);
      cnv.close_path();
   }, {{50, 50}, {50, 80}});
   CHECK(hit[0]);      // on the closing diagonal
   CHECK(!hit[1]);
}

TEST_CASE("current path: Queries", "[current_path]")
{
   on_canvas([](canvas& cnv)
   {
      cnv.add_rect(10, 10, 40, 40);
      CHECK(cnv.point_in_path(point{25, 25}));
      CHECK(cnv.point_in_path(25, 25));
      CHECK(!cnv.point_in_path(point{5, 5}));
      CHECK(near(cnv.fill_extent(), rect{10, 10, 50, 50}));
   });
}

TEST_CASE("current path: Fill Rule", "[current_path]")
{
   on_canvas([](canvas& cnv)
   {
      // Two nested squares wound the same way. Under winding the hole is
      // filled; under odd-even it is not.
      auto nested = [](canvas& cnv)
      {
         cnv.add_rect(0, 0, 100, 100);
         cnv.add_rect(25, 25, 50, 50);
      };

      cnv.fill_rule(path::fill_winding);
      nested(cnv);
      CHECK(cnv.point_in_path(point{50, 50}));
      cnv.begin_path();

      cnv.fill_rule(path::fill_odd_even);
      nested(cnv);
      CHECK(!cnv.point_in_path(point{50, 50}));

      // The rule outlives begin_path: on Cairo and Quartz 2D it is canvas
      // state, not path state. (On Skia begin_path resets it.)
      cnv.begin_path();
      nested(cnv);
      CHECK(!cnv.point_in_path(point{50, 50}));
   });

   on_canvas([](canvas& cnv)
   {
      // On Cairo the rule is part of the saved state. Quartz 2D keeps it
      // outside the state stack, so there it survives a restore.
      {
         auto s = cnv.new_state();
         cnv.fill_rule(path::fill_odd_even);
      }
      cnv.add_rect(0, 0, 100, 100);
      cnv.add_rect(25, 25, 50, 50);
      CHECK(cnv.point_in_path(point{50, 50}));
   });
}

TEST_CASE("current path: clear_rect current behaviour", "[current_path]")
{
   // BEHAVIOUR UNDER REVIEW. On Cairo, clear_rect appends its rectangle to
   // the current path and fills the lot with the CLEAR operator, so it both
   // erases whatever geometry was already being built and discards the path.
   // Quartz 2D (CGContextClearRect) and Skia (drawRect) leave the current
   // path alone.
   on_canvas([](canvas& cnv)
   {
      cnv.add_rect(0, 0, 10, 10);
      cnv.clear_rect(50, 50, 10, 10);
      CHECK(cnv.fill_extent() == rect{0, 0, 0, 0});
   });
}

#endif // ARTIST_CAIRO

namespace
{
   color const ghost = rgba(176, 176, 176, 255);   // #b0b0b0
   color const accent = rgba(21, 101, 192, 255);   // #1565c0
   color const annot = rgba(93, 93, 93, 255);      // #5d5d5d

   void caption(canvas& cnv, char const* s, float x, float y)
   {
      cnv.font(font_descr{"Open Sans", 15});
      cnv.fill_style(colors::black);
      cnv.text_align(canvas::center | canvas::baseline);
      cnv.fill_text(s, x, y);
   }

   // A self-intersecting five-pointed star traced as one continuous path.
   void star(canvas& cnv, point c, float r)
   {
      auto const step = 4 * std::numbers::pi_v<float> / 5;   // skip a vertex
      for (int i = 0; i != 5; ++i)
      {
         auto a = -std::numbers::pi_v<float> / 2 + i * step;
         auto p = point{c.x + r * std::cos(a), c.y + r * std::sin(a)};
         if (i == 0)
            cnv.move_to(p);
         else
            cnv.line_to(p);
      }
      cnv.close_path();
   }
}

TEST_CASE("current path: arc figure", "[current_path]")
{
   // The page figure images/canvas/arc.png: the same start and end angle
   // swept both ways, on a y-down canvas.
   float const w = 560, h = 235;
   image img{w, h, 2};
   {
      offscreen_image ctx{img};
      canvas cnv{ctx.context()};
      cnv.fill_style(colors::white);
      cnv.fill_rect(0, 0, w, h);

      auto const start_angle = -std::numbers::pi_v<float> / 6;
      auto const end_angle = std::numbers::pi_v<float> / 2;
      char const* names[] = {"ccw = false (the default)", "ccw = true"};

      for (int i = 0; i != 2; ++i)
      {
         auto c = point{150.0f + i * 260.0f, 95.0f};
         float const r = 62;

         // The circle the arc runs on, and the two radii.
         cnv.begin_path();
         cnv.stroke_style(ghost);
         cnv.line_width(1.25);
         cnv.add_circle(c.x, c.y, r);
         cnv.stroke();

         cnv.stroke_style(annot);
         for (auto a : {start_angle, end_angle})
         {
            cnv.move_to(c);
            cnv.line_to(c.x + r * std::cos(a), c.y + r * std::sin(a));
         }
         cnv.move_to(c);
         cnv.line_to(c.x + r + 18, c.y);         // angle 0 points along +x
         cnv.stroke();

         cnv.stroke_style(accent);
         cnv.line_width(3.5);
         cnv.begin_path();
         cnv.arc(c, r, start_angle, end_angle, i == 1);
         cnv.stroke();

         cnv.font(font_descr{"Open Sans", 15});
         cnv.fill_style(colors::black);
         cnv.text_align(canvas::left | canvas::middle);
         cnv.fill_text("0", c.x + r + 24, c.y);
         cnv.fill_text("start", c.x + r * std::cos(start_angle) + 6,
            c.y + r * std::sin(start_angle) - 8);
         cnv.text_align(canvas::center | canvas::middle);
         cnv.fill_text("end", c.x, c.y + r + 18);

         caption(cnv, names[i], c.x, 222);
      }
   }
   img.save_png(get_results_path() + "canvas_arc.png");
}

TEST_CASE("current path: fill_rule figure", "[current_path]")
{
   // The page figure images/canvas/fill_rule.png: one self-intersecting star
   // under each rule.
   float const w = 560, h = 215;
   image img{w, h, 2};
   {
      offscreen_image ctx{img};
      canvas cnv{ctx.context()};
      cnv.fill_style(colors::white);
      cnv.fill_rect(0, 0, w, h);

      char const* names[] = {"path::fill_winding", "path::fill_odd_even"};
      path::fill_rule_enum rules[] = {path::fill_winding, path::fill_odd_even};

      for (int i = 0; i != 2; ++i)
      {
         auto c = point{150.0f + i * 260.0f, 95.0f};

         cnv.begin_path();
         cnv.fill_rule(rules[i]);
         star(cnv, c, 72);
         cnv.fill_style(accent);
         cnv.stroke_style(annot);
         cnv.line_width(1.25);
         cnv.fill_preserve();
         cnv.stroke();

         caption(cnv, names[i], c.x, 202);
      }
   }
   img.save_png(get_results_path() + "canvas_fill_rule.png");
}
