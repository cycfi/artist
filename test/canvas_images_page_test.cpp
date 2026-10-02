/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]

   Asserts the claims of docs/modules/ROOT/pages/canvas/images.adoc and draws
   its figures. Each case names the page section it comes from.

   Every probe renders onto a transparent image and samples pixels, so a
   pixel (x, y) covers [x, x+1) by [y, y+1). Sample points sit a pixel or two
   clear of an edge, away from the antialiased boundary.
=============================================================================*/
#include "test_support.hpp"
#include <artist/canvas_layer.hpp>
#include <artist/path.hpp>

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

   bool clear(rgba8 p)     { return p.a < 30; }
   bool is_red(rgba8 p)    { return p.r > 200 && p.g < 60 && p.b < 60; }
   bool is_blue(rgba8 p)   { return p.b > 200 && p.r < 60 && p.g < 60; }
   bool is_green(rgba8 p)  { return p.g > 200 && p.r < 60 && p.b < 60; }

   template <typename F>
   void render(image& img, F f)
   {
      offscreen_image ctx{img};
      canvas cnv{ctx.context()};
      f(cnv);
   }

   // The probe source: 4 by 4 units, left half red, right half blue. Small
   // enough that a draw into a larger rect is unmistakably a scale.
   image make_source(float scale = 1)
   {
      image src{4, 4, scale};
      render(src, [](canvas& cnv)
      {
         cnv.fill_style(rgba(255, 0, 0, 255));
         cnv.fill_rect(rect{0, 0, 2, 4});
         cnv.fill_style(rgba(0, 0, 255, 255));
         cnv.fill_rect(rect{2, 0, 4, 4});
      });
      return src;
   }
}

TEST_CASE("canvas images: Overview", "[images]")
{
   {
      // draw paints the image and nothing else: the current path is neither
      // painted with it nor disturbed by it.
      image dst{60, 60, 1};
      auto src = make_source();
      render(dst, [&](canvas& cnv)
      {
         cnv.fill_style(rgba(0, 255, 0, 255));
         cnv.add_rect(rect{0, 0, 20, 20});         // built, not painted
         cnv.draw(src, rect{30, 30, 50, 50});
         cnv.fill();                               // still there to paint
      });

#if !defined(ARTIST_RECORDING)
      CHECK(is_green(pixel_at(dst, 10, 10)));      // the path survived
      CHECK(is_red(pixel_at(dst, 35, 40)));        // the image landed
#endif
   }

   {
      // The ink is the image's own. The fill style does not tint it.
      image dst{60, 60, 1};
      auto src = make_source();
      render(dst, [&](canvas& cnv)
      {
         cnv.fill_style(rgba(0, 255, 0, 255));
         cnv.draw(src, rect{10, 10, 50, 50});
      });

#if !defined(ARTIST_RECORDING)
      CHECK(is_red(pixel_at(dst, 15, 30)));
      CHECK(is_blue(pixel_at(dst, 45, 30)));
#endif
   }

   {
      // The geometry goes through the current transform.
      image dst{60, 60, 1};
      auto src = make_source();
      render(dst, [&](canvas& cnv)
      {
         cnv.translate(20, 20);
         cnv.scale(2, 2);
         cnv.draw(src, point{0, 0});               // 4 by 4 at 2x, at (20, 20)
      });

#if !defined(ARTIST_RECORDING)
      CHECK(!clear(pixel_at(dst, 22, 22)));
      CHECK(clear(pixel_at(dst, 30, 30)));         // past 8 units
      CHECK(clear(pixel_at(dst, 10, 10)));         // before the translate
#endif
   }

   {
      // The clip applies as it does to any other paint.
      image dst{60, 60, 1};
      auto src = make_source();
      render(dst, [&](canvas& cnv)
      {
         cnv.add_rect(rect{0, 0, 30, 60});
         cnv.clip();
         cnv.draw(src, rect{0, 0, 60, 60});
      });

#if !defined(ARTIST_RECORDING)
      CHECK(!clear(pixel_at(dst, 15, 30)));
      CHECK(clear(pixel_at(dst, 45, 30)));
#endif
   }

   {
      // The image is composited over what is already there, alpha and all.
      image src{4, 4, 1};
      render(src, [](canvas& cnv)
      {
         cnv.fill_style(rgba(255, 0, 0, 128));
         cnv.fill_rect(rect{0, 0, 4, 4});
      });

      image dst{60, 60, 1};
      render(dst, [&](canvas& cnv)
      {
         cnv.fill_style(rgba(0, 0, 255, 255));
         cnv.fill_rect(rect{0, 0, 60, 60});
         cnv.draw(src, rect{10, 10, 50, 50});
      });

#if !defined(ARTIST_RECORDING)
      auto p = pixel_at(dst, 30, 30);
      CHECK(std::abs(p.r - 128) <= 4);
      CHECK(std::abs(p.b - 127) <= 4);
      CHECK(p.a == 255);
#endif
   }

   {
      // The shadow and the compositing operator apply too.
      image dst{60, 60, 1};
      auto src = make_source();
      render(dst, [&](canvas& cnv)
      {
         cnv.shadow_style(point{6, 6}, 0, rgba(0, 0, 0, 255));
         cnv.draw(src, rect{10, 10, 30, 30});
      });

#if !defined(ARTIST_RECORDING)
      auto p = pixel_at(dst, 33, 33);              // below and right of it
      CHECK(p.a > 200);
      CHECK(p.r < 40);
      CHECK(p.g < 40);
      CHECK(p.b < 40);
#endif
   }

   {
      image dst{60, 60, 1};
      auto src = make_source();
      render(dst, [&](canvas& cnv)
      {
         cnv.fill_style(rgba(0, 255, 0, 255));
         cnv.fill_rect(rect{0, 0, 60, 60});
         cnv.global_composite_operation(canvas::destination_over);
         cnv.draw(src, rect{10, 10, 50, 50});
      });

#if !defined(ARTIST_RECORDING)
      CHECK(is_green(pixel_at(dst, 30, 30)));      // the ground wins
#endif
   }
}

TEST_CASE("canvas images: Placing an Image", "[images]")
{
   {
      // draw(pic) puts the top left corner at the origin.
      image dst{20, 20, 1};
      auto src = make_source();
      render(dst, [&](canvas& cnv) { cnv.draw(src); });

#if !defined(ARTIST_RECORDING)
      CHECK(is_red(pixel_at(dst, 1, 1)));
      CHECK(clear(pixel_at(dst, 6, 6)));           // past size()
#endif
   }

   {
      // draw(pic, pos), draw(pic, posx, posy) and the src/dest form with the
      // whole image at its own size all paint the same pixels.
      auto place = [](image& dst, int which)
      {
         auto src = make_source();
         render(dst, [&](canvas& cnv)
         {
            switch (which)
            {
               case 0: cnv.draw(src, point{5, 6}); break;
               case 1: cnv.draw(src, 5.0f, 6.0f); break;
               case 2: cnv.draw(src, rect{0, 0, 4, 4}, rect{5, 6, 9, 10}); break;
            }
         });
      };

      image a{20, 20, 1}, b{20, 20, 1}, c{20, 20, 1};
      place(a, 0); place(b, 1); place(c, 2);

#if !defined(ARTIST_RECORDING)
      for (int y = 0; y != 20; ++y)
      {
         for (int x = 0; x != 20; ++x)
         {
            auto pa = pixel_at(a, x, y);
            auto pb = pixel_at(b, x, y);
            auto pc = pixel_at(c, x, y);
            REQUIRE(pa.a == pb.a);
            REQUIRE(pa.r == pb.r);
            REQUIRE(pa.a == pc.a);
            REQUIRE(pa.r == pc.r);
         }
      }

      // It landed where it was asked to, at 4 by 4 units.
      CHECK(clear(pixel_at(a, 3, 8)));             // left of it
      CHECK(is_red(pixel_at(a, 6, 8)));
      CHECK(is_blue(pixel_at(a, 8, 8)));
      CHECK(clear(pixel_at(a, 10, 8)));            // right of it
#endif
   }

   {
      // The size drawn is size(), the image's own units, not bitmap_size().
      // A 2x image covers the same ground as a 1x one of the same size().
      auto src = make_source(2);
      CHECK(src.size().x == 4);
      CHECK(src.bitmap_size().x == 8);

      image dst{20, 20, 1};
      render(dst, [&](canvas& cnv) { cnv.draw(src, point{0, 0}); });

#if !defined(ARTIST_RECORDING)
      CHECK(!clear(pixel_at(dst, 2, 2)));
      CHECK(clear(pixel_at(dst, 6, 6)));
#endif
   }
}

TEST_CASE("canvas images: Scaling an Image", "[images]")
{
   {
      // draw(pic, pos, scale) multiplies size() by scale. The two scalar and
      // point forms agree.
      image a{40, 40, 1}, b{40, 40, 1};
      auto src = make_source();
      render(a, [&](canvas& cnv) { cnv.draw(src, point{4, 4}, 3.0f); });
      render(b, [&](canvas& cnv) { cnv.draw(src, 4.0f, 4.0f, 3.0f); });

#if !defined(ARTIST_RECORDING)
      for (int y = 0; y != 40; ++y)
         for (int x = 0; x != 40; ++x)
            REQUIRE(pixel_at(a, x, y).a == pixel_at(b, x, y).a);

      CHECK(is_red(pixel_at(a, 8, 14)));           // 4 to 16, left half
      CHECK(is_blue(pixel_at(a, 14, 14)));         // right half
      CHECK(clear(pixel_at(a, 18, 18)));           // past 4 + 4 * 3
#endif
   }

   {
      // draw(pic, dest) fills dest whatever its shape: the aspect ratio is
      // not kept, and the two axes stretch independently.
      image dst{80, 40, 1};
      auto src = make_source();
      render(dst, [&](canvas& cnv) { cnv.draw(src, rect{0, 0, 80, 20}); });

#if !defined(ARTIST_RECORDING)
      CHECK(is_red(pixel_at(dst, 10, 10)));
      CHECK(is_blue(pixel_at(dst, 70, 10)));
      CHECK(clear(pixel_at(dst, 40, 30)));         // below dest, nothing
#endif
   }

   {
      // An empty dest paints nothing.
      image dst{60, 60, 1};
      auto src = make_source();
      render(dst, [&](canvas& cnv)
      {
         cnv.fill_style(rgba(0, 255, 0, 255));
         cnv.fill_rect(rect{0, 0, 60, 60});
         cnv.draw(src, rect{10, 10, 10, 40});      // zero width
      });

#if !defined(ARTIST_RECORDING)
      CHECK(is_green(pixel_at(dst, 10, 20)));
      CHECK(is_green(pixel_at(dst, 30, 30)));
#endif
   }

   {
      // Scaling is smoothed, not nearest neighbour: the seam between the two
      // halves is a gradient a few pixels wide once the image is blown up.
      image dst{80, 20, 1};
      auto src = make_source();
      render(dst, [&](canvas& cnv) { cnv.draw(src, rect{0, 0, 80, 20}); });

#if !defined(ARTIST_RECORDING)
      auto before = pixel_at(dst, 38, 10);
      auto after = pixel_at(dst, 42, 10);
      CHECK(before.r > 100);                       // still mostly red
      CHECK(before.b > 30);                        // but blue has bled in
      CHECK(after.b > 100);
      CHECK(after.r > 30);
#endif
   }
}

TEST_CASE("canvas images: Drawing Part of an Image", "[images]")
{
   {
      // src selects a region in the image's own units and dest is where it
      // goes. Taking the right half puts blue across the whole of dest.
      image dst{60, 60, 1};
      auto src = make_source();
      render(dst, [&](canvas& cnv)
      {
         cnv.draw(src, rect{2, 0, 4, 4}, rect{0, 0, 60, 60});
      });

#if !defined(ARTIST_RECORDING)
      CHECK(is_blue(pixel_at(dst, 10, 30)));
      CHECK(is_blue(pixel_at(dst, 50, 30)));
#endif
   }

   {
      // src is in units, so it means the same region of a 2x image as of a
      // 1x one.
      auto one = make_source(1);
      auto two = make_source(2);

      image a{60, 60, 1}, b{60, 60, 1};
      render(a, [&](canvas& cnv)
      {
         cnv.draw(one, rect{0, 0, 2, 4}, rect{0, 0, 60, 60});
      });
      render(b, [&](canvas& cnv)
      {
         cnv.draw(two, rect{0, 0, 2, 4}, rect{0, 0, 60, 60});
      });

#if !defined(ARTIST_RECORDING)
      CHECK(is_red(pixel_at(a, 30, 30)));
      CHECK(is_red(pixel_at(b, 30, 30)));
#endif
   }
}

TEST_CASE("canvas images: Drawing a Layer", "[images]")
{
   {
      // draw(layer, pos) puts the layer down at layer.size(); draw(layer,
      // dest) stretches it to fill dest.
      image dst{60, 60, 1};
      render(dst, [&](canvas& cnv)
      {
         canvas_layer layer{cnv, extent{20, 20}};
         CHECK(layer.size().x == 20);
         CHECK(layer.size().y == 20);
         {
            canvas lc{layer.context()};
            lc.fill_style(rgba(255, 0, 0, 255));
            lc.fill_rect(rect{0, 0, 20, 20});
         }
         cnv.draw(layer, rect{0, 0, 40, 40});
      });

#if !defined(ARTIST_RECORDING)
      CHECK(is_red(pixel_at(dst, 30, 30)));        // stretched past 20
      CHECK(clear(pixel_at(dst, 50, 50)));         // and stopped at 40
#endif
   }

   {
      // A layer follows the canvas's density: its size() stays in units, and
      // it lands at those units however dense the canvas is.
      image dst{60, 60, 2};                        // a 2x canvas
      render(dst, [&](canvas& cnv)
      {
         canvas_layer layer{cnv, extent{20, 20}};
         CHECK(layer.size().x == 20);
         {
            canvas lc{layer.context()};
            lc.fill_style(rgba(255, 0, 255, 255));
            lc.fill_rect(rect{0, 0, 20, 20});
         }
         cnv.draw(layer, point{5, 5});             // units 5 to 25
      });

#if !defined(ARTIST_RECORDING)
      CHECK(!clear(pixel_at(dst, 20, 20)));        // pixel 20 is unit 10
      CHECK(clear(pixel_at(dst, 56, 56)));         // pixel 56 is unit 28
#endif
   }
}

///////////////////////////////////////////////////////////////////////////////
// Current behaviour. These are not claims the page makes; they pin down what
// the backends do today so a change is visible. Each is a difference from the
// W3C canvas API raised with Joel on 2026-09-18.

TEST_CASE("canvas images: current behaviour, src past the image edge", "[images]")
{
   // W3C clips src to the image and clips dest in the same proportion, which
   // is what Cairo does. Quartz 2D clips src and then stretches the smaller
   // region across the whole of dest.
   image dst{60, 60, 1};
   auto src = make_source();
   render(dst, [&](canvas& cnv)
   {
      // src is 6 units wide; only its first 2 are in the image.
      cnv.draw(src, rect{2, 0, 8, 4}, rect{0, 0, 60, 30});
   });

#if defined(ARTIST_CAIRO)
   CHECK(is_blue(pixel_at(dst, 10, 15)));          // the third that exists
   CHECK(clear(pixel_at(dst, 30, 15)));            // the overrun is empty
   CHECK(clear(pixel_at(dst, 50, 15)));
#elif defined(ARTIST_QUARTZ_2D)
   CHECK(is_blue(pixel_at(dst, 10, 15)));
   CHECK(is_blue(pixel_at(dst, 30, 15)));          // stretched over dest
   CHECK(is_blue(pixel_at(dst, 50, 15)));
#endif
}

TEST_CASE("canvas images: current behaviour, empty src", "[images]")
{
   // W3C paints nothing when the source rectangle has zero width or height,
   // which is what Cairo does. On Quartz 2D the empty sub-image falls back to
   // the whole image, which is then stretched across dest.
   image dst{60, 60, 1};
   auto src = make_source();
   render(dst, [&](canvas& cnv)
   {
      cnv.fill_style(rgba(0, 255, 0, 255));
      cnv.fill_rect(rect{0, 0, 60, 60});
      cnv.draw(src, rect{2, 0, 2, 4}, rect{10, 10, 50, 50});
   });

#if defined(ARTIST_CAIRO)
   CHECK(is_green(pixel_at(dst, 20, 30)));         // nothing was painted
   CHECK(is_green(pixel_at(dst, 40, 30)));
#elif defined(ARTIST_QUARTZ_2D)
   CHECK(is_red(pixel_at(dst, 20, 30)));           // the whole image, stretched
   CHECK(is_blue(pixel_at(dst, 40, 30)));
#endif
}

TEST_CASE("canvas images: current behaviour, reversed dest", "[images]")
{
   // A dest whose right is left of its left. W3C's drawImage takes a width,
   // and a negative one mirrors the image; Artist takes a rect. Cairo mirrors
   // it, Quartz 2D normalises the rectangle and does not.
   image dst{60, 60, 1};
   auto src = make_source();
   render(dst, [&](canvas& cnv)
   {
      cnv.draw(src, rect{50, 10, 10, 50});
   });

#if defined(ARTIST_CAIRO)
   CHECK(is_blue(pixel_at(dst, 20, 30)));          // mirrored
   CHECK(is_red(pixel_at(dst, 40, 30)));
#elif defined(ARTIST_QUARTZ_2D)
   CHECK(is_red(pixel_at(dst, 20, 30)));           // not mirrored
   CHECK(is_blue(pixel_at(dst, 40, 30)));
#endif
}

///////////////////////////////////////////////////////////////////////////////
// The page figures.

namespace
{
   // The page's == Example opens with this, so the helper on the page is the
   // helper that draws every figure on it.
   void draw_digit(canvas& cnv, int n, point at)
   {
      // Three segment shapes in a 16 by 26 digit: the horizontal bar across
      // the top, the vertical post down one side, and the pointed middle
      // bar. The bar is 16 long and the post 13, so they are not one shape
      // turned; the other four segments are these two turned half a turn.
      static path const bar{"M 0.5 0 L 15.5 0 L 13.1 2.4 L 2.9 2.4 Z"};
      static path const post{"M 0 0.5 L 0 12.5 L 2.4 10.1 L 2.4 2.9 Z"};
      static path const mid{
         "M 0 13 L 1.2 11.8 L 14.8 11.8 L 16 13 L 14.8 14.2 L 1.2 14.2 Z"
      };

      // Segments a to g: the shape, where it goes, and whether it is turned.
      struct placement { path const& shape; float x, y; bool turned; };
      static placement const seg[] =
      {
         {bar, 0, 0, false},   {post, 16, 13, true},  {post, 16, 26, true},
         {bar, 16, 26, true},  {post, 0, 13, false},  {post, 0, 0, false},
         {mid, 0, 0, false}
      };
      static int const lit[] =
         {0x3f, 0x06, 0x5b, 0x4f, 0x66, 0x6d, 0x7d, 0x07, 0x7f, 0x6f};

      auto st = cnv.new_state();
      cnv.fill_style(rgba(26, 26, 26, 255));
      cnv.fill_round_rect(rect{at.x, at.y, at.x + 32, at.y + 32}, 4);
      cnv.translate(at.x + 8, at.y + 3);

      for (int i = 0; i != 7; ++i)
      {
         auto seg_state = cnv.new_state();
         cnv.translate(seg[i].x, seg[i].y);
         if (seg[i].turned)
            cnv.rotate(cycfi::pi);
         cnv.fill_style(
            ((lit[n] >> i) & 1)?
               rgba(255, 255, 255, 255) : rgba(255, 255, 255, 12)
         );
         cnv.add_path(seg[i].shape);
         cnv.fill();
      }
   }

   color const ghost = rgba(176, 176, 176, 255);   // #b0b0b0
   color const ink = rgba(26, 26, 26, 255);        // #1a1a1a

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

   // The figures' source image: four 32 by 32 cells of a seven segment
   // display, reading 0 to 3, so the cell at src offset 64 shows a 2. It is
   // a 4x image, so the cell blown up to 96 units in the src and dest figure
   // still reads. The Example's own sheet is 1x, as a reader's would be.
   image make_strip()
   {
      image strip{128, 32, 4};
      {
         offscreen_image ctx{strip};
         canvas cnv{ctx.context()};
         for (int i = 0; i != 4; ++i)
            draw_digit(cnv, i, point{i * 32.0f, 0});
      }
      return strip;
   }
}

TEST_CASE("canvas images: placement figure", "[images]")
{
   // The page figure images/canvas/draw_placement.png: the same image placed
   // three ways, at its own size, scaled, and stretched into a rectangle.
   figure(200, "canvas_draw_placement.png", [](canvas& cnv)
   {
      auto pic = make_strip();                     // 128 by 32

      cnv.draw(pic, point{14, 40});
      caption(cnv, "draw(pic, pos)", 78, 140);

      cnv.draw(pic, point{184, 40}, 1.5f);
      caption(cnv, "draw(pic, pos, 1.5)", 280, 140);

      auto dest = rect{418, 40, 546, 104};
      guide_rect(cnv, dest);
      cnv.draw(pic, dest);
      caption(cnv, "draw(pic, dest)", 482, 140);

      caption(cnv, "the image is 128 by 32, and dest is 128 by 64", 280, 178);
   });
}

TEST_CASE("canvas images: src and dest figure", "[images]")
{
   // The page figure images/canvas/draw_src_dest.png: one cell lifted out of
   // the strip and drawn larger.
   figure(220, "canvas_draw_src_dest.png", [](canvas& cnv)
   {
      auto pic = make_strip();

      cnv.draw(pic, point{40, 40});
      guide_rect(cnv, rect{40 + 64, 40, 40 + 96, 72});
      caption(cnv, "src = rect{64, 0, 96, 32}", 104, 104);

      auto dest = rect{340, 30, 436, 126};
      guide_rect(cnv, dest);
      cnv.draw(pic, rect{64, 0, 96, 32}, dest);
      caption(cnv, "dest = rect{340, 30, 436, 126}", 388, 158);

      // The arrow from the marked cell to dest.
      {
         auto st = cnv.new_state();
         cnv.line_width(1.25);
         cnv.stroke_style(rgba(93, 93, 93, 255));
         cnv.move_to(point{215, 78});
         cnv.line_to(point{308, 78});
         cnv.stroke();
         cnv.fill_style(rgba(93, 93, 93, 255));
         cnv.move_to(point{320, 78});
         cnv.line_to(point{308, 73});
         cnv.line_to(point{308, 83});
         cnv.fill();
      }

      caption(cnv, "src is in the image's own units", 280, 200);
   });
}

namespace
{
   // The page's == Example, verbatim, so the page's code is the code that
   // draws the page's figure and the code the assertions below check. It
   // opens with draw_digit above, which the page shows first.
   void example(canvas& cnv)
   {
      // The ten digits, rendered once into one 320 by 32 sheet.
      auto sheet = image{320, 32};
      {
         offscreen_image ctx{sheet};
         canvas scnv{ctx.context()};
         for (int d = 0; d != 10; ++d)
            draw_digit(scnv, d, point{d * 32.0f, 0});
      }

      // A reading, blitted from the sheet one digit at a time. The 2 is drawn
      // twice, from the one cell that holds it.
      cnv.draw(sheet, point{20, 20});
      char const* reading = "2026";
      for (int i = 0; reading[i]; ++i)
      {
         float x = (reading[i] - '0') * 32.0f;
         cnv.draw(
            sheet,
            rect{x, 0, x + 32, 32},
            rect{60 + i * 64.0f, 70, 108 + i * 64.0f, 118}
         );
      }
   }
}

TEST_CASE("canvas images: Example", "[images]")
{
   image img{360, 138, 1};
   render(img, [](canvas& cnv) { example(cnv); });

#if !defined(ARTIST_RECORDING)
   // The sheet landed at its own size: ten cells across 320 units from 20.
   CHECK(!clear(pixel_at(img, 36, 36)));           // inside cell 0
   CHECK(!clear(pixel_at(img, 324, 36)));          // inside cell 9
   CHECK(clear(pixel_at(img, 345, 36)));           // past the sheet's right
   CHECK(clear(pixel_at(img, 36, 62)));            // below it

   // The reading is four 48 unit cells from x 60, spaced 64 apart, so each
   // cell is painted and the gaps between them are clear.
   for (int i = 0; i != 4; ++i)
   {
      CHECK(!clear(pixel_at(img, 84 + i * 64, 94)));
      if (i != 3)
         CHECK(clear(pixel_at(img, 116 + i * 64, 94)));
   }

   // The 2 is blitted twice, so the first and third cells of the reading are
   // the same drawing. Both land on whole units, so the resampling is the
   // same phase; a count of 1 or 2 apart is the resampler's own arithmetic.
   // Every cell carries the same opaque panel, so the comparison has to be
   // on colour: alpha alone cannot tell one digit from another.
   auto same_pixel =
      [](rgba8 a, rgba8 b)
      {
         return std::abs(a.r - b.r) <= 2 && std::abs(a.g - b.g) <= 2
            && std::abs(a.b - b.b) <= 2 && std::abs(a.a - b.a) <= 2;
      };

   // Skia resamples the two blits at slightly different subpixel phases, so
   // the glyph edges land differently: 106 of the 2304 pixels differ, by up
   // to 176, while Cairo and Quartz 2D differ in none. The interior is what
   // identifies the drawing, so allow a tenth of the cell to differ, which
   // is twice what Skia needs and far less than a different digit would.
   int differing = 0;
   for (int y = 0; y != 48; ++y)
      for (int x = 0; x != 48; ++x)
         if (!same_pixel(
               pixel_at(img, 60 + x, 70 + y),
               pixel_at(img, 188 + x, 70 + y)))
            ++differing;
   CHECK(differing * 10 <= 48 * 48);

   // The second cell is a 0 and the first a 2, so they differ.
   bool same = true;
   for (int y = 0; y != 48 && same; ++y)
      for (int x = 0; x != 48 && same; ++x)
         if (!same_pixel(
               pixel_at(img, 60 + x, 70 + y),
               pixel_at(img, 124 + x, 70 + y)))
            same = false;
   CHECK(!same);
#endif
}

TEST_CASE("canvas images: example figure", "[images]")
{
   // The page figure images/canvas/images_example.png: what the code under
   // == Example draws, shifted right to sit centred in the figure's width.
   figure(138, "canvas_images_example.png", [](canvas& cnv)
   {
      cnv.translate(100, 0);
      example(cnv);
   });
}

