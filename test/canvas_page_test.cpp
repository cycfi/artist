/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]

   Asserts the claims of docs/modules/ROOT/pages/canvas.adoc, the Canvas
   overview, and draws its figure. Each case names the page section it comes
   from.

   The overview states in prose what the topic pages state call by call, so
   the cases here are deliberately the overview's own claims only: the
   drawing order, the coordinate system, and what the canvas carries between
   calls. The per-call behaviour is asserted by each topic page's test.

   Every probe renders onto a transparent 100 by 100 image and samples
   pixels, so a pixel (x, y) covers [x, x+1) by [y, y+1). Sample points sit
   a pixel or two clear of an edge, away from the antialiased boundary.
=============================================================================*/
#include "test_support.hpp"

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

   bool inked(rgba8 p)     { return p.a > 200; }
   bool blank(rgba8 p)     { return p.a < 30; }
   bool reddish(rgba8 p)   { return p.r > 200 && p.g < 80 && p.b < 80; }
   bool bluish(rgba8 p)    { return p.b > 150 && p.r < 80; }

   template <typename F>
   void render(image& img, F f)
   {
      offscreen_image ctx{img};
      canvas cnv{ctx.context()};
      f(cnv);
   }
}

///////////////////////////////////////////////////////////////////////////////
// == Overview / === Drawing Is Immediate

TEST_CASE("canvas overview: a later call paints over an earlier one",
   "[canvas_page]")
{
   // "A drawing is the sum of the calls made, in the order they were made."
   // Nothing stands for the first rectangle once its fill returns, so the
   // second one simply lands on top of it.
   image img{100, 100, 1};
   render(img, [](canvas& cnv)
   {
      cnv.fill_style(colors::red);
      cnv.fill_rect(10, 10, 60, 60);
      cnv.fill_style(colors::blue);
      cnv.fill_rect(40, 40, 50, 50);
   });

#if !defined(ARTIST_RECORDING)
   CHECK(reddish(pixel_at(img, 20, 20)));    // only the first
   CHECK(bluish(pixel_at(img, 55, 55)));     // the second, over the first
   CHECK(bluish(pixel_at(img, 80, 80)));     // only the second
#endif
}

///////////////////////////////////////////////////////////////////////////////
// == Overview / === One Call, Five Steps

TEST_CASE("canvas overview: the five steps apply to one paint",
   "[canvas_page]")
{
   // The transform, the fill style, the shadow and the clip are all in
   // effect before the fill, and all four decide what it lays down.
   image img{100, 100, 1};
   render(img, [](canvas& cnv)
   {
      auto st = cnv.new_state();
      cnv.add_circle(circle{50, 50, 30});
      cnv.clip();
      cnv.translate(50, 50);
      cnv.fill_style(colors::blue);
      cnv.shadow_style({0, 0}, 6, rgba(0, 0, 0, 255));
      cnv.add_rect(-40, -40, 80, 80);
      cnv.fill();
   });

#if !defined(ARTIST_RECORDING)
   // The transform placed it and the style painted it.
   CHECK(bluish(pixel_at(img, 50, 50)));

   // The clip is the last word: the square is 80 units across and the clip
   // circle is 60, so the square's own corner never reaches the canvas.
   CHECK(blank(pixel_at(img, 14, 14)));
   CHECK(blank(pixel_at(img, 86, 86)));
#endif
}

TEST_CASE("canvas overview: fill and stroke consume the path", "[canvas_page]")
{
   // "After either returns the canvas is building nothing." A second fill
   // with no second add_rect paints nothing at all.
   image img{100, 100, 1};
   render(img, [](canvas& cnv)
   {
      cnv.fill_style(colors::red);
      cnv.add_rect(10, 10, 30, 30);
      cnv.fill();

      cnv.translate(50, 50);
      cnv.fill_style(colors::blue);
      cnv.fill();                   // there is no path left to fill
   });

#if !defined(ARTIST_RECORDING)
   CHECK(reddish(pixel_at(img, 25, 25)));
   CHECK(blank(pixel_at(img, 75, 75)));
#endif
}

///////////////////////////////////////////////////////////////////////////////
// == Overview / === Coordinates and Units

TEST_CASE("canvas overview: the origin is the top left and y grows down",
   "[canvas_page]")
{
   image img{100, 100, 1};
   render(img, [](canvas& cnv)
   {
      cnv.fill_style(colors::red);
      cnv.fill_rect(0, 0, 20, 20);        // the origin corner
      cnv.fill_style(colors::blue);
      cnv.fill_rect(70, 80, 20, 20);      // larger x is right, larger y down
   });

#if !defined(ARTIST_RECORDING)
   CHECK(reddish(pixel_at(img, 5, 5)));
   CHECK(bluish(pixel_at(img, 80, 90)));
   CHECK(blank(pixel_at(img, 5, 90)));
   CHECK(blank(pixel_at(img, 90, 5)));
#endif
}

TEST_CASE("canvas overview: coordinates are units, not pixels",
   "[canvas_page]")
{
   // The same drawing code on a scale 2 canvas covers the same ground in
   // twice as many pixels.
   auto square = [](canvas& cnv)
   {
      cnv.fill_style(colors::blue);
      cnv.fill_rect(0, 0, 25, 25);
   };

   image one{50, 50, 1};
   render(one, square);
   image two{50, 50, 2};
   render(two, square);

#if !defined(ARTIST_RECORDING)
   CHECK(one.bitmap_size() == extent{50, 50});
   CHECK(two.bitmap_size() == extent{100, 100});

   // A quarter of the canvas on both, which is 25 pixels on one and 50 on
   // the other.
   CHECK(bluish(pixel_at(one, 20, 20)));
   CHECK(blank(pixel_at(one, 30, 30)));
   CHECK(bluish(pixel_at(two, 45, 45)));
   CHECK(blank(pixel_at(two, 55, 55)));
#endif
}

TEST_CASE("canvas overview: edges are antialiased", "[canvas_page]")
{
   // A circle's edge is blended into what is behind it rather than snapped,
   // so the render carries partly covered pixels all round it. Counted over
   // the whole image: the row through the centre is the one place the edge
   // is vertical and lands square on a pixel boundary.
   image img{100, 100, 1};
   render(img, [](canvas& cnv)
   {
      cnv.fill_style(colors::black);
      cnv.add_circle(circle{50, 50, 30});
      cnv.fill();
   });

#if !defined(ARTIST_RECORDING)
   int partial = 0;
   for (int y = 0; y != 100; ++y)
      for (int x = 0; x != 100; ++x)
      {
         auto a = pixel_at(img, x, y).a;
         if (a > 20 && a < 235)
            ++partial;
      }
   CHECK(partial > 100);
#endif
}

TEST_CASE("canvas overview: a hairline on a unit boundary straddles two rows",
   "[canvas_page]")
{
   // The page's advice about a 1 unit line. Centred on a whole unit it
   // spans half of each neighbouring row; centred on a half unit it fills
   // one row.
   image img{100, 100, 1};
   render(img, [](canvas& cnv)
   {
      cnv.stroke_style(colors::black);
      cnv.line_width(1);
      cnv.move_to(10, 30);  cnv.line_to(90, 30);    // on a whole unit
      cnv.stroke();
      cnv.move_to(10, 70.5f);  cnv.line_to(90, 70.5f);   // on a half unit
      cnv.stroke();
   });

#if !defined(ARTIST_RECORDING)
   auto soft_hi = pixel_at(img, 50, 29).a;
   auto soft_lo = pixel_at(img, 50, 30).a;
   CHECK(soft_hi > 80);
   CHECK(soft_hi < 200);
   CHECK(soft_lo > 80);
   CHECK(soft_lo < 200);

   CHECK(pixel_at(img, 50, 70).a > 240);      // the crisp one, one full row
   CHECK(pixel_at(img, 50, 69).a < 30);
   CHECK(pixel_at(img, 50, 71).a < 30);
#endif
}

///////////////////////////////////////////////////////////////////////////////
// == Overview / === The Canvas Carries State

TEST_CASE("canvas overview: a style set once applies to what follows",
   "[canvas_page]")
{
   // "A fill colour set once applies to every fill that follows until
   // another is set."
   image img{100, 100, 1};
   render(img, [](canvas& cnv)
   {
      cnv.fill_style(colors::red);
      cnv.fill_rect(10, 10, 30, 30);
      cnv.fill_rect(60, 60, 30, 30);      // no second fill_style
   });

#if !defined(ARTIST_RECORDING)
   CHECK(reddish(pixel_at(img, 25, 25)));
   CHECK(reddish(pixel_at(img, 75, 75)));
#endif
}

///////////////////////////////////////////////////////////////////////////////
// The page figure.

namespace
{
   color const guide = rgba(204, 204, 204, 255);   // #cccccc
   color const slate = rgba(93, 93, 93, 255);      // #5d5d5d
   color const page_ink = rgba(26, 26, 26, 255);   // #1a1a1a
   color const accent = rgba(21, 101, 192, 255);   // #1565c0
   color const amber = rgba(255, 179, 0, 255);     // #ffb300

   float const cell = 100, gap = 12, left_margin = 6, panel_top = 8;

   float cell_x(int i) { return left_margin + i * (cell + gap); }

   // The one shape every panel shows, in panel-local coordinates.
   rect const shape = {22, 28, 78, 72};
   float const shape_radius = 10;

   void panel_frame(canvas& cnv)
   {
      cnv.stroke_style(guide);
      cnv.line_width(1);
      cnv.stroke_rect(0.5f, 0.5f, cell - 1, cell - 1);
   }

   // The transform the figure shows: a turn about the panel's centre.
   void turn(canvas& cnv)
   {
      cnv.translate(cell / 2, cell / 2);
      cnv.rotate(-0.35f);
      cnv.translate(-cell / 2, -cell / 2);
   }

   void outline(canvas& cnv, color c)
   {
      cnv.stroke_style(c);
      cnv.line_width(1.5f);
      cnv.add_round_rect(shape, shape_radius);
      cnv.stroke();
   }

   void solid(canvas& cnv)
   {
      cnv.fill_style(accent);
      cnv.add_round_rect(shape, shape_radius);
      cnv.fill();
   }

   void label(canvas& cnv, char const* s, int i)
   {
      auto st = cnv.new_state();
      cnv.font(font_descr{"Open Sans", 15});
      cnv.fill_style(page_ink);
      cnv.text_align(canvas::center);
      cnv.fill_text(s, cell_x(i) + cell / 2, panel_top + cell + 26);
   }
}

TEST_CASE("canvas overview: drawing model figure", "[canvas_page]")
{
   // The page figure images/canvas/drawing_model.png: one fill call shown
   // at each of the five steps of the drawing model, each panel a real
   // render of that step.
   float const w = 560, h = 150;
   image img{w, h, 2};
   {
      offscreen_image ctx{img};
      canvas cnv{ctx.context()};
      cnv.fill_style(colors::white);
      cnv.fill_rect(0, 0, w, h);

      // 1. The path: geometry built into the canvas, nothing painted.
      {
         auto st = cnv.new_state();
         cnv.translate(cell_x(0), panel_top);
         panel_frame(cnv);
         outline(cnv, slate);
      }

      // 2. The transform: the same path, mapped onto the surface. The pale
      // outline is where it sat with no transform.
      {
         auto st = cnv.new_state();
         cnv.translate(cell_x(1), panel_top);
         panel_frame(cnv);
         outline(cnv, guide);
         turn(cnv);
         outline(cnv, slate);
      }

      // 3. The paint: the fill style colours the shape.
      {
         auto st = cnv.new_state();
         cnv.translate(cell_x(2), panel_top);
         panel_frame(cnv);
         turn(cnv);
         solid(cnv);
      }

      // 4. The shadow: cast behind the shape, offset and blurred.
      {
         auto st = cnv.new_state();
         cnv.translate(cell_x(3), panel_top);
         panel_frame(cnv);
         cnv.shadow_style({5, 5}, 8, rgba(0, 0, 0, 120));
         turn(cnv);
         solid(cnv);
      }

      // 5. The composite: combined with what is already on the canvas, and
      // only inside the clip.
      {
         auto st = cnv.new_state();
         cnv.translate(cell_x(4), panel_top);
         panel_frame(cnv);

         cnv.fill_style(amber);              // already on the canvas
         cnv.fill_rect(1, 44, cell - 2, 16);

         auto hole = circle{cell / 2 - 9, cell / 2 - 7, 32};
         cnv.stroke_style(guide);            // where the clip runs
         cnv.line_width(1.5f);
         cnv.add_circle(hole);
         cnv.stroke();

         auto inner = cnv.new_state();
         cnv.add_circle(hole);
         cnv.clip();
         cnv.shadow_style({5, 5}, 8, rgba(0, 0, 0, 120));
         turn(cnv);
         solid(cnv);
      }

      label(cnv, "path", 0);
      label(cnv, "transform", 1);
      label(cnv, "paint", 2);
      label(cnv, "shadow", 3);
      label(cnv, "composite", 4);
   }

#if !defined(ARTIST_RECORDING)
   // The figure is at scale 2, so a sample in drawing units is two pixels.
   auto at = [&img](float x, float y)
   {
      return pixel_at(img, int(2 * x), int(2 * y));
   };

   // Panel 1 is an outline: its middle is the white ground, not ink.
   CHECK(blank(at(cell_x(0) + cell / 2, panel_top + cell / 2)) == false);
   CHECK(at(cell_x(0) + cell / 2, panel_top + cell / 2).r > 240);

   // Panel 3 is solid accent where panel 1 is bare.
   CHECK(bluish(at(cell_x(2) + cell / 2, panel_top + cell / 2)));

   // Panel 5 clips: the amber bar survives outside the clip circle, and
   // the shape does not reach the panel's corner.
   auto bar = at(cell_x(4) + 6, panel_top + 52);
   CHECK(bar.r > 200);
   CHECK(bar.g > 140);
   CHECK(bar.b < 90);
   CHECK(!bluish(at(cell_x(4) + 6, panel_top + 14)));
#endif

   img.save_png(get_results_path() + "canvas_drawing_model.png");
}
