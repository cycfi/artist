/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]

   Asserts the claims of docs/modules/ROOT/pages/canvas.adoc and draws its
   figure. Each case names the page section it comes from.

   Every probe renders onto a transparent 100 by 100 image and samples
   pixels, so a pixel (x, y) covers [x, x+1) by [y, y+1). Sample points sit
   a pixel or two clear of an edge, away from the antialiased boundary.

   The page's claim that a canvas cannot be copied, moved or assigned is
   checked by the static_asserts under == Construction and Lifetime rather
   than by a case: a deleted function is a compile error, not a runtime
   result.
=============================================================================*/
#include "test_support.hpp"
#include <type_traits>

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
   bool bluish(rgba8 p)    { return p.b > 150 && p.r < 80; }
}

///////////////////////////////////////////////////////////////////////////////
// == Expressions / === Construction and Lifetime

// "It cannot be copied, moved or assigned either, so it is passed by
// reference and nothing else."
static_assert(!std::is_copy_constructible_v<canvas>);
static_assert(!std::is_move_constructible_v<canvas>);
static_assert(!std::is_copy_assignable_v<canvas>);
static_assert(!std::is_move_assignable_v<canvas>);

// "Explicit: a canvas_impl* does not convert to a canvas on its own." And
// there is no other way in: no default constructor.
static_assert(std::is_constructible_v<canvas, canvas_impl*>);
static_assert(!std::is_convertible_v<canvas_impl*, canvas>);
static_assert(!std::is_default_constructible_v<canvas>);

// == Validity: "Explicit, so it converts in a condition and nowhere else."
static_assert(std::is_constructible_v<bool, canvas&>);
static_assert(!std::is_convertible_v<canvas&, bool>);

TEST_CASE("canvas page: the canvas does not own its context", "[canvas_page]")
{
   // Destroying the canvas does not destroy the context. A second canvas
   // built on the same context draws, and the first one's ink is still
   // there when it does.
   image img{100, 100, 1};
   {
      offscreen_image ctx{img};
      {
         canvas cnv{ctx.context()};
         cnv.fill_style(colors::red);
         cnv.fill_rect(10, 10, 30, 30);
      }
      {
         canvas cnv{ctx.context()};
         cnv.fill_style(colors::blue);
         cnv.fill_rect(60, 10, 30, 30);
      }
   }

#if !defined(ARTIST_RECORDING)
   auto first = pixel_at(img, 25, 25);
   CHECK(inked(first));
   CHECK(first.r > 200);
   CHECK(first.b < 80);

   auto second = pixel_at(img, 75, 25);
   CHECK(inked(second));
   CHECK(second.b > 200);
   CHECK(second.r < 80);
#endif
}

TEST_CASE("canvas page: the context keeps what the last call left",
   "[canvas_page]")
{
   // "Destroying the canvas does not put the context back the way it was
   // found." The first canvas translates the context and dies; the second
   // takes that as its origin, so a rect it draws at (0, 0) lands at the
   // translated position.
   image img{100, 100, 1};
   {
      offscreen_image ctx{img};
      {
         canvas cnv{ctx.context()};
         cnv.translate(40, 40);
      }
      {
         canvas cnv{ctx.context()};
         cnv.fill_style(colors::blue);
         cnv.fill_rect(0, 0, 20, 20);
      }
   }

#if !defined(ARTIST_RECORDING)
   CHECK(bluish(pixel_at(img, 50, 50)));     // inside the translated rect
   CHECK(blank(pixel_at(img, 10, 10)));      // nothing at the untranslated one
#endif
}

///////////////////////////////////////////////////////////////////////////////
// == Expressions / === Validity

TEST_CASE("canvas page: a canvas on a live context is valid", "[canvas_page]")
{
   image img{100, 100, 1};
   offscreen_image ctx{img};
   canvas cnv{ctx.context()};

   CHECK(bool(cnv));
   CHECK_FALSE(!cnv);
}

#if defined(ARTIST_QUARTZ_2D) || defined(ARTIST_RECORDING)
TEST_CASE("canvas page: a null context is invalid (current behaviour)",
   "[canvas_page]")
{
   // The page's CAUTION. Only a canvas built from a null canvas_impl* is
   // invalid, and only these two backends survive building one: the Cairo
   // constructor dereferences the pointer (a measured segmentation fault),
   // and the Skia and Direct2D constructors do the same, read but not run.
   // So this cannot be a portable case, and the operators below cannot be
   // reached at all on three of the five backends.
   canvas cnv{nullptr};

   CHECK_FALSE(bool(cnv));
   CHECK(!cnv);
   CHECK(cnv.impl() == nullptr);
}
#endif

///////////////////////////////////////////////////////////////////////////////
// == Expressions / === Backend Access

TEST_CASE("canvas page: impl is the pointer that went in", "[canvas_page]")
{
   image img{100, 100, 1};
   offscreen_image ctx{img};
   canvas cnv{ctx.context()};

   CHECK(cnv.impl() == ctx.context());
}

///////////////////////////////////////////////////////////////////////////////
// == Example

namespace
{
   // The page's == Example, verbatim, so the page's code is the code that
   // draws the page's figure and the code the assertions below check.
   void draw(canvas& cnv)
   {
      auto bounds = cnv.clip_extent();
      auto dot = circle{center_point(bounds), 40};

      // Set the style, build the path, then paint it.
      cnv.fill_style(rgba(21, 101, 192, 255));
      cnv.add_round_rect(bounds.inset(20), 12);
      cnv.fill();

      cnv.fill_style(colors::white);
      cnv.add_circle(dot);
      cnv.fill();

      // The fill above consumed the circle, so it is added again here.
      cnv.stroke_style(rgba(26, 26, 26, 255));
      cnv.line_width(5);
      cnv.add_circle(dot);
      cnv.stroke();
   }
}

TEST_CASE("canvas page: Example and its figure", "[canvas_page]")
{
   // The page figure images/canvas/canvas_example.png. The example sizes
   // itself to the canvas it is given, so the figure and the probe are one
   // render, at the width the page places the image.
   float const w = 560, h = 200;
   image img{w, h, 2};
   {
      offscreen_image ctx{img};
      canvas cnv{ctx.context()};
      cnv.fill_style(colors::white);
      cnv.fill_rect(0, 0, w, h);
      draw(cnv);
   }

#if !defined(ARTIST_RECORDING)
   // The image is at scale 2, so a sample in drawing units is two pixels.
   auto at = [&img](int x, int y) { return pixel_at(img, 2 * x, 2 * y); };

   // The panel is inset 20 from the canvas, which the function read from
   // clip_extent() rather than being told.
   CHECK(bluish(at(280, 30)));               // inside the panel's top edge
   CHECK(!bluish(at(280, 10)));              // outside it, on white ground

   // The disc is white over the panel.
   auto disc = at(280, 100);
   CHECK(inked(disc));
   CHECK(disc.r > 200);
   CHECK(disc.g > 200);
   CHECK(disc.b > 200);

   // The ring is dark, and it is a stroke over the disc's own circle, so it
   // sits centred on the circumference 40 units out rather than beside it.
   auto ring = at(280, 60);
   CHECK(inked(ring));
   CHECK(ring.r < 70);
   CHECK(ring.g < 70);
   CHECK(ring.b < 70);
#endif

   img.save_png(get_results_path() + "canvas_example.png");
}
