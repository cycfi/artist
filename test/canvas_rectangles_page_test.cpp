/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]

   Asserts the claims of docs/modules/ROOT/pages/canvas/rectangles.adoc and
   draws its figures. Each case names the page section it comes from.

   Every probe renders onto a transparent 100 by 100 image and samples
   pixels, so a pixel (x, y) covers [x, x+1) by [y, y+1). Sample points sit
   a pixel or two clear of an edge, away from the antialiased boundary.
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

   bool inked(rgba8 p)   { return p.a > 200; }
   bool clear(rgba8 p)   { return p.a < 30; }

   template <typename F>
   void render(image& img, F f)
   {
      offscreen_image ctx{img};
      canvas cnv{ctx.context()};
      f(cnv);
   }

   color const ink_color = rgba(255, 0, 0, 255);
}

TEST_CASE("canvas rectangles: Overview", "[rectangles]")
{
   {
      // fill_rect(r) is add_rect(r) followed by fill(): the two paint the
      // same pixels.
      image one{100, 100, 1};
      render(one, [](canvas& cnv)
      {
         cnv.fill_style(ink_color);
         cnv.fill_rect(rect{20, 30, 80, 70});
      });

      image two{100, 100, 1};
      render(two, [](canvas& cnv)
      {
         cnv.fill_style(ink_color);
         cnv.add_rect(rect{20, 30, 80, 70});
         cnv.fill();
      });

#if !defined(ARTIST_RECORDING)
      for (int y = 0; y < 100; y += 7)
         for (int x = 0; x < 100; x += 7)
            REQUIRE(pixel_at(one, x, y).a == pixel_at(two, x, y).a);
#endif
   }

   {
      // They paint the rectangle alone. A shape half built into the current
      // path is not painted with it, as in the W3C API.
      image img{100, 100, 1};
      render(img, [](canvas& cnv)
      {
         cnv.fill_style(ink_color);
         cnv.add_rect(5, 5, 20, 20);         // built, not painted
         cnv.fill_rect(60, 60, 30, 30);
      });
#if !defined(ARTIST_RECORDING)
      CHECK(clear(pixel_at(img, 15, 15)));   // the half-built shape, unpainted
      CHECK(inked(pixel_at(img, 75, 75)));   // the rectangle asked for
#endif
   }

   {
      // And they leave the current path as they found it: what was being
      // built is still there afterwards, and a later fill() paints it.
      image img{100, 100, 1};
      render(img, [](canvas& cnv)
      {
         cnv.fill_style(ink_color);
         cnv.add_rect(5, 5, 20, 20);
         cnv.fill_rect(60, 60, 30, 30);
         cnv.fill_style(rgba(0, 0, 255, 255));
         cnv.fill();                         // paints the shape, still there
      });
#if !defined(ARTIST_RECORDING)
      auto kept = pixel_at(img, 15, 15);
      CHECK(kept.b > 200);                   // the kept shape, painted blue
      CHECK(kept.r < 60);
      auto box = pixel_at(img, 75, 75);
      CHECK(box.r > 200);                    // the rectangle, its own color
      CHECK(box.b < 60);
#endif
   }

   {
      // The same for the round forms and for stroking, on every backend.
      for (int which = 0; which != 4; ++which)
      {
         image img{100, 100, 1};
         render(img, [which](canvas& cnv)
         {
            cnv.fill_style(ink_color);
            cnv.stroke_style(ink_color);
            cnv.line_width(4);
            cnv.add_rect(5, 5, 20, 20);
            switch (which)
            {
               case 0: cnv.fill_rect(rect{60, 60, 90, 90}); break;
               case 1: cnv.fill_round_rect(rect{60, 60, 90, 90}, 8); break;
               case 2: cnv.stroke_rect(rect{60, 60, 90, 90}); break;
               case 3: cnv.stroke_round_rect(rect{60, 60, 90, 90}, 8); break;
            }
            cnv.fill_style(rgba(0, 0, 255, 255));
            cnv.fill();
         });
#if !defined(ARTIST_RECORDING)
         auto kept = pixel_at(img, 15, 15);
         CHECK(kept.b > 200);                // the path survived the call
         CHECK(kept.r < 60);
#endif
      }
   }

   {
      // The geometry goes through the current transform, like any other
      // shape.
      image img{100, 100, 1};
      render(img, [](canvas& cnv)
      {
         cnv.fill_style(ink_color);
         cnv.translate(50, 0);
         cnv.fill_rect(10, 10, 30, 30);      // device x 60 to 90
      });
#if !defined(ARTIST_RECORDING)
      CHECK(clear(pixel_at(img, 25, 25)));
      CHECK(inked(pixel_at(img, 75, 25)));
#endif
   }

   {
      // The shadow style applies, as it does to any other fill.
      image img{100, 100, 1};
      render(img, [](canvas& cnv)
      {
         cnv.fill_style(ink_color);
         cnv.shadow_style({8, 8}, 0, rgba(0, 0, 255, 255));
         cnv.fill_rect(rect{20, 20, 60, 60});
      });
#if !defined(ARTIST_RECORDING)
      auto p = pixel_at(img, 65, 65);        // in the shadow, past the fill
      CHECK(p.b > 200);
      CHECK(p.r < 60);
#endif
   }
}

TEST_CASE("canvas rectangles: Filling", "[rectangles]")
{
   {
      // The scalar form takes a position and a size, not two corners.
      image img{100, 100, 1};
      render(img, [](canvas& cnv)
      {
         cnv.fill_style(ink_color);
         cnv.fill_rect(20, 20, 40, 40);      // covers 20 to 60
      });
#if !defined(ARTIST_RECORDING)
      CHECK(inked(pixel_at(img, 55, 55)));
      CHECK(clear(pixel_at(img, 65, 65)));
#endif
   }

   {
      // A radius of zero, and any negative radius, gives square corners.
      for (float radius : {0.0f, -10.0f})
      {
         image img{100, 100, 1};
         render(img, [radius](canvas& cnv)
         {
            cnv.fill_style(ink_color);
            cnv.fill_round_rect(rect{20, 20, 80, 80}, radius);
         });
#if !defined(ARTIST_RECORDING)
         CHECK(inked(pixel_at(img, 21, 21)));   // the corner is filled
#endif
      }
   }

   {
      // radius is clamped to half the shorter side: an over-large radius
      // gives a stadium, never a shape larger than r. The rect is 80 by 40,
      // so the clamp is 20.
      image img{100, 100, 1};
      render(img, [](canvas& cnv)
      {
         cnv.fill_style(ink_color);
         cnv.fill_round_rect(rect{10, 30, 90, 70}, 1000);
      });
#if !defined(ARTIST_RECORDING)
      CHECK(clear(pixel_at(img, 11, 31)));    // the corner is gone
      CHECK(inked(pixel_at(img, 11, 50)));    // the middle of the end cap
      CHECK(inked(pixel_at(img, 50, 31)));    // the flat between the caps
      CHECK(clear(pixel_at(img, 50, 25)));    // nothing above the rect
      CHECK(clear(pixel_at(img, 5, 50)));     // nothing left of it
#endif
   }

   {
      // The clamp is on the shorter side, not each side in turn: an 80 by
      // 20 rect clamps to 10, so 20 in from the left the top edge is flat.
      image img{100, 100, 1};
      render(img, [](canvas& cnv)
      {
         cnv.fill_style(ink_color);
         cnv.fill_round_rect(rect{10, 40, 90, 60}, 40);
      });
#if !defined(ARTIST_RECORDING)
      CHECK(inked(pixel_at(img, 11, 50)));    // middle of the end cap
      CHECK(inked(pixel_at(img, 30, 41)));    // flat top, past the cap
#endif
   }

   {
      // fill_round_rect(r, 0) paints what fill_rect(r) paints.
      image one{100, 100, 1};
      render(one, [](canvas& cnv)
      {
         cnv.fill_style(ink_color);
         cnv.fill_round_rect(rect{20, 30, 80, 70}, 0);
      });

      image two{100, 100, 1};
      render(two, [](canvas& cnv)
      {
         cnv.fill_style(ink_color);
         cnv.fill_rect(rect{20, 30, 80, 70});
      });

#if !defined(ARTIST_RECORDING)
      for (int y = 0; y < 100; y += 7)
         for (int x = 0; x < 100; x += 7)
            REQUIRE(pixel_at(one, x, y).a == pixel_at(two, x, y).a);
#endif
   }

   {
      // An empty rectangle fills nothing.
      image img{100, 100, 1};
      render(img, [](canvas& cnv)
      {
         cnv.fill_style(ink_color);
         cnv.fill_rect(rect{20, 50, 80, 50});      // zero height
         cnv.fill_rect(rect{50, 20, 50, 80});      // zero width
      });
#if !defined(ARTIST_RECORDING)
      CHECK(clear(pixel_at(img, 40, 50)));
      CHECK(clear(pixel_at(img, 50, 40)));
#endif
   }
}

TEST_CASE("canvas rectangles: Stroking", "[rectangles]")
{
   {
      // The outline is centred on the edge: half the line width falls
      // inside the rectangle and half outside. A 10 wide stroke on an edge
      // at x 30 inks 25 to 35.
      image img{100, 100, 1};
      render(img, [](canvas& cnv)
      {
         cnv.stroke_style(ink_color);
         cnv.line_width(10);
         cnv.stroke_rect(rect{30, 30, 70, 70});
      });
#if !defined(ARTIST_RECORDING)
      CHECK(clear(pixel_at(img, 23, 50)));    // clear outside the band
      CHECK(inked(pixel_at(img, 26, 50)));    // outer half
      CHECK(inked(pixel_at(img, 33, 50)));    // inner half
      CHECK(clear(pixel_at(img, 37, 50)));    // clear inside the band
      CHECK(clear(pixel_at(img, 50, 50)));    // the middle is not filled
#endif
   }

   {
      // The scalar form takes a position and a size here too.
      image img{100, 100, 1};
      render(img, [](canvas& cnv)
      {
         cnv.stroke_style(ink_color);
         cnv.line_width(4);
         cnv.stroke_rect(20, 20, 40, 40);     // edges at 20 and 60
      });
#if !defined(ARTIST_RECORDING)
      CHECK(inked(pixel_at(img, 60, 40)));    // the right edge, at 60
      CHECK(clear(pixel_at(img, 80, 40)));
#endif
   }

   {
      // radius is clamped the same way as for a fill.
      image img{100, 100, 1};
      render(img, [](canvas& cnv)
      {
         cnv.stroke_style(ink_color);
         cnv.line_width(4);
         cnv.stroke_round_rect(rect{10, 30, 90, 70}, 1000);
      });
#if !defined(ARTIST_RECORDING)
      CHECK(clear(pixel_at(img, 11, 31)));    // the corner is rounded away
      CHECK(inked(pixel_at(img, 10, 50)));    // the end cap, at the clamp
      CHECK(inked(pixel_at(img, 50, 30)));    // the flat top between caps
#endif
   }

   {
      // Corners rounded to a radius above zero are arcs, so line_join has
      // nothing left to shape, but it still shapes them at radius zero.
      auto probe = [](float radius, canvas::join_enum join)
      {
         image img{100, 100, 1};
         render(img, [radius, join](canvas& cnv)
         {
            cnv.stroke_style(ink_color);
            cnv.line_width(16);
            cnv.line_join(join);
            cnv.stroke_round_rect(rect{30, 30, 70, 70}, radius);
         });
         // The outer tip of the top left corner: inked under a miter join,
         // cut away under a round one.
         return inked(pixel_at(img, 23, 23));
      };

#if !defined(ARTIST_RECORDING)
      CHECK(probe(0, canvas::miter_join));
      CHECK_FALSE(probe(0, canvas::round_join));
      // Above zero the corner is already an arc, so the join makes no
      // difference to it.
      CHECK(probe(10, canvas::miter_join) == probe(10, canvas::round_join));
#endif
   }

   {
      // Unlike a fill, an empty rectangle still strokes: a zero height
      // rectangle draws as a line of the stroke's width.
      image img{100, 100, 1};
      render(img, [](canvas& cnv)
      {
         cnv.stroke_style(ink_color);
         cnv.line_width(6);
         cnv.stroke_rect(rect{20, 50, 80, 50});
      });
#if !defined(ARTIST_RECORDING)
      CHECK(inked(pixel_at(img, 50, 50)));
      CHECK(inked(pixel_at(img, 50, 48)));
      CHECK(clear(pixel_at(img, 50, 40)));
#endif
   }
}

TEST_CASE("canvas rectangles: add_round_rect appends", "[rectangles]")
{
   // add_round_rect adds to the current path, it does not replace it, so a
   // rounded shape and a plain one built together fill as one path. Quartz
   // 2D used to begin a new path here and drop the first shape; fixed
   // 2026-09-17 with Joel's ruling to follow the W3C API.
   image img{100, 100, 1};
   render(img, [](canvas& cnv)
   {
      cnv.fill_style(ink_color);
      cnv.add_rect(5, 5, 20, 20);
      cnv.add_round_rect(rect{60, 60, 90, 90}, 8);
      cnv.fill();
   });

#if !defined(ARTIST_RECORDING)
   CHECK(inked(pixel_at(img, 15, 15)));       // the first shape, kept
   CHECK(inked(pixel_at(img, 75, 75)));       // the rounded one
#endif
}

///////////////////////////////////////////////////////////////////////////////
// The page figures.

namespace
{
   color const ghost = rgba(176, 176, 176, 255);   // #b0b0b0
   color const slate = rgba(93, 93, 93, 255);      // #5d5d5d
   color const ink = rgba(26, 26, 26, 255);        // #1a1a1a
   color const accent = rgba(21, 101, 192, 255);   // #1565c0

   void caption(canvas& cnv, char const* s, float x, float y)
   {
      auto st = cnv.new_state();
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

   // A thin grey outline of r, the geometry the call was given.
   void guide_rect(canvas& cnv, rect r)
   {
      auto st = cnv.new_state();
      cnv.line_width(1.25);
      cnv.stroke_style(ghost);
      cnv.stroke_rect(r);
   }
}

TEST_CASE("canvas rectangles: radius figure", "[rectangles]")
{
   // The page figure images/canvas/round_rect_radius.png: one rectangle
   // filled at four radii, the last one past the clamp.
   figure(180, "canvas_round_rect_radius.png", [](canvas& cnv)
   {
      struct probe
      {
         char const* name;
         char const* note;
         float       radius;
      };

      // Each box is 112 by 72, so the clamp is 36.
      probe const probes[] = {
         {"radius 0", nullptr, 0},
         {"radius 8", nullptr, 8},
         {"radius 24", nullptr, 24},
         {"radius 1000", "clamped to 36", 1000},
      };

      for (int i = 0; i != 4; ++i)
      {
         auto left = 22.0f + i * 132;
         auto box = rect{left, 40, left + 112, 112};
         cnv.fill_style(accent);
         cnv.fill_round_rect(box, probes[i].radius);
         caption(cnv, probes[i].name, left + 56, 142);
         if (probes[i].note)
            caption(cnv, probes[i].note, left + 56, 164);
      }
   });
}

TEST_CASE("canvas rectangles: stroke figure", "[rectangles]")
{
   // The page figure images/canvas/stroke_rect.png: a fill stops at the
   // edge; a stroke is centred on it, so the same rectangle covers a
   // different band. The grey outline in both panels is the rectangle the
   // call was given.
   figure(200, "canvas_stroke_rect.png", [](canvas& cnv)
   {
      auto a = rect{60, 40, 240, 140};
      cnv.fill_style(accent);
      cnv.fill_rect(a);
      guide_rect(cnv, a);
      caption(cnv, "fill_rect: ink stops at the edge", 150, 175);

      auto b = rect{320, 40, 500, 140};
      cnv.stroke_style(accent);
      cnv.line_width(16);
      cnv.stroke_rect(b);
      guide_rect(cnv, b);
      caption(cnv, "stroke_rect: ink straddles it", 410, 175);
   });
}

namespace
{
   // The page's == Example, verbatim, so the page's code is the code that
   // draws the page's figure and the code the assertions below check.
   void example(canvas& cnv)
   {
      auto frame = rect{20, 20, 280, 100};
      cnv.stroke_style(rgba(93, 93, 93, 255));
      cnv.line_width(2);
      cnv.stroke_rect(frame);

      auto track = rect{72, 38, 192, 82};
      cnv.fill_style(rgba(21, 101, 192, 255));
      cnv.fill_round_rect(track, 1000);      // clamped to a stadium

      cnv.fill_style(colors::white);
      cnv.fill_round_rect(rect{152, 42, 188, 78}, 1000);

      cnv.font(font_descr{"Open Sans", 15});
      cnv.fill_style(rgba(26, 26, 26, 255));
      cnv.text_align(canvas::left | canvas::middle);
      cnv.fill_text("On", 208, 60);
   }
}

TEST_CASE("canvas rectangles: Example", "[rectangles]")
{
   image img{300, 120, 1};
   render(img, [](canvas& cnv) { example(cnv); });

#if !defined(ARTIST_RECORDING)
   // The frame is an outline: ink on the edge, nothing just inside it.
   CHECK(inked(pixel_at(img, 20, 60)));
   CHECK(clear(pixel_at(img, 40, 60)));

   // The track is a stadium, 120 by 44, so the clamp is 22: its corner is
   // empty and the middle of its end cap is painted.
   CHECK(clear(pixel_at(img, 73, 39)));
   CHECK(inked(pixel_at(img, 73, 60)));

   // The knob sits at the right end of the track and is white, not blue.
   auto knob = pixel_at(img, 170, 60);
   CHECK(knob.r > 200);
   CHECK(knob.g > 200);
   CHECK(knob.b > 200);

   // The track is blue where the knob is not.
   auto bar = pixel_at(img, 100, 60);
   CHECK(bar.b > 150);
   CHECK(bar.r < 100);

   // The drawing is centred in the frame: equal gaps left and right, and
   // the track the same distance from the top and the bottom.
   CHECK(clear(pixel_at(img, 60, 60)));       // left gap, before the track
   CHECK(clear(pixel_at(img, 240, 60)));      // right gap, after the label
   CHECK(clear(pixel_at(img, 130, 30)));      // above the track
   CHECK(clear(pixel_at(img, 130, 90)));      // below it, by the same margin
   CHECK(inked(pixel_at(img, 130, 40)));      // the track's top edge
   CHECK(inked(pixel_at(img, 130, 80)));      // and its bottom edge
#endif
}

TEST_CASE("canvas rectangles: example figure", "[rectangles]")
{
   // The page figure images/canvas/rectangles_example.png: what the code
   // under == Example draws. The example spans x 20 to 280, so it is
   // shifted right to sit centred in the figure's width.
   figure(120, "canvas_rectangles_example.png", [](canvas& cnv)
   {
      cnv.translate(130, 0);
      example(cnv);
   });
}
