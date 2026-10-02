/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]

   Draws the == Example figures for the reference pages whose only drawing
   claim is the example itself, so they need a backend but not a page test
   of their own: foundation/circle, foundation/color, foundation/font,
   foundation/rect and path. Pages that already have a page test keep their
   example there.

   Each example() is the page's code verbatim, so the code on the page is
   the code that draws the page's figure. The figure wrapper supplies only
   the ground and the shift that centres the drawing, never any of the
   drawing itself.
=============================================================================*/
#include "test_support.hpp"
#include <array>

namespace
{
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

   // The red, green and blue of one pixel of a 2x figure.
   std::array<int, 3> at(image const& img, int x, int y)
   {
      auto p = reinterpret_cast<std::uint8_t const*>(img.pixels());
      p += 4 * (2 * y * int(img.bitmap_size().x) + 2 * x);
      return {p[2], p[1], p[0]};
   }

   bool blank(std::array<int, 3> c)
   {
      return c[0] > 240 && c[1] > 240 && c[2] > 240;
   }
}

///////////////////////////////////////////////////////////////////////////////
// foundation/circle.adoc

namespace
{
   void circle_example(canvas& cnv)
   {
      auto bounds = rect{0, 0, 200, 140};
      auto dial = circle{center_point(bounds), 40};

      cnv.add_circle(dial);
      cnv.fill_style(colors::gray[20]);
      cnv.fill();

      cnv.add_circle(dial.inset(6));
      cnv.stroke_style(colors::light_steel_blue);
      cnv.line_width(2);
      cnv.stroke();
   }
}

TEST_CASE("page examples: circle", "[page-examples]")
{
   float const w = 560, h = 110;
   image img{w, h, 2};
   {
      offscreen_image ctx{img};
      canvas cnv{ctx.context()};
      cnv.fill_style(colors::white);
      cnv.fill_rect(0, 0, w, h);
      cnv.translate(180, -15);
      circle_example(cnv);
   }
   img.save_png(get_results_path() + "circle_example.png");

#if !defined(ARTIST_RECORDING)
   // Under the shift the dial is centred at 280, 55 with a radius of 40,
   // and the ring it strokes sits 6 in from the rim.
   CHECK(at(img, 280, 55)[0] < 80);       // the dark disc
   CHECK(blank(at(img, 280, 5)));         // clear of the disc
   // The ring is lighter than the disc it sits on.
   CHECK(at(img, 280, 21)[2] > at(img, 280, 55)[2]);
#endif
}

///////////////////////////////////////////////////////////////////////////////
// foundation/color.adoc

namespace
{
   void color_example(canvas& cnv)
   {
      constexpr auto panel = colors::dark_slate_blue;
      constexpr auto hover = panel.level(1.8f);

      auto swatch = [&](rect bounds, bool hovered)
      {
         cnv.fill_style(colors::gray[20]);
         cnv.fill_rect(bounds);

         cnv.fill_style((hovered? hover : panel).opacity(0.6f));
         cnv.fill_rect(bounds);
      };

      swatch(rect{40, 20, 240, 110}, false);
      swatch(rect{280, 20, 480, 110}, true);
   }
}

TEST_CASE("page examples: color", "[page-examples]")
{
   float const w = 560, h = 140;
   image img{w, h, 2};
   {
      offscreen_image ctx{img};
      canvas cnv{ctx.context()};
      cnv.fill_style(colors::white);
      cnv.fill_rect(0, 0, w, h);
      cnv.translate(20, 0);
      color_example(cnv);

      // Scaffolding, not part of the example: which swatch is which.
      cnv.font(font_descr{"Open Sans", 15});
      cnv.fill_style(rgba(26, 26, 26, 255));
      cnv.text_align(canvas::center | canvas::baseline);
      cnv.fill_text("panel", 140, 132);
      cnv.fill_text("hover", 380, 132);
   }
   img.save_png(get_results_path() + "color_example.png");

#if !defined(ARTIST_RECORDING)
   // level(1.2) brightens, so the hovered swatch on the right is lighter
   // than the plain one on the left in every channel.
   auto plain = at(img, 160, 65);
   auto hot = at(img, 400, 65);
   CHECK(hot[0] > plain[0]);
   CHECK(hot[1] > plain[1]);
   CHECK(hot[2] > plain[2]);
   // Both sit at 60 percent over the same dark ground, so neither is the
   // flat color it names.
   CHECK(!blank(plain));
   CHECK(plain[2] > plain[0]);      // still blue leaning
#endif
}

///////////////////////////////////////////////////////////////////////////////
// foundation/font.adoc

namespace
{
   void font_example(canvas& cnv)
   {
      float x = 40, y = 50;
      auto heading = font{font_descr{"Open Sans, Helvetica", 24}.bold()};

      cnv.font(heading);
      cnv.fill_style(colors::black);
      cnv.fill_text("Artist", {x, y});

      auto lh = heading.line_height();
      auto dx = heading.measure_text("Artist")
              - heading.measure_text("2D Canvas");

      cnv.fill_text("2D Canvas", {x + dx, y + lh});
   }
}

TEST_CASE("page examples: font", "[page-examples]")
{
   float const w = 560, h = 120;
   image img{w, h, 2};
   {
      offscreen_image ctx{img};
      canvas cnv{ctx.context()};
      cnv.fill_style(colors::white);
      cnv.fill_rect(0, 0, w, h);
      cnv.translate(190, 0);
      font_example(cnv);
   }
   img.save_png(get_results_path() + "font_example.png");

#if !defined(ARTIST_RECORDING)
   // The rightmost ink on each of the two lines. The second line is
   // shifted by the difference of the two measured widths, so the two
   // lines end at the same x: that is what right aligning it means.
   auto right_edge = [&img](int y0, int y1)
   {
      auto p = reinterpret_cast<std::uint8_t const*>(img.pixels());
      int iw = int(img.bitmap_size().x);
      int r = -1;
      for (int y = 2 * y0; y != 2 * y1; ++y)
         for (int x = 0; x != iw; ++x)
            if (p[4 * (y * iw + x) + 2] < 128)
               r = std::max(r, x);
      return r;
   };
   auto first = right_edge(20, 60);
   auto second = right_edge(60, 100);
   REQUIRE(first > 0);      // both lines drew ink
   REQUIRE(second > 0);
   CHECK(std::abs(first - second) <= 4);
#endif
}

///////////////////////////////////////////////////////////////////////////////
// foundation/rect.adoc

namespace
{
   void rect_example(canvas& cnv)
   {
      auto bounds = rect{20, 20, 320, 140};
      auto clip = rect{0, 0, 282, 160};

      // The card.
      cnv.fill_style(colors::gray[92]);
      cnv.fill_round_rect(bounds, 6);
      cnv.stroke_style(colors::gray[70]);
      cnv.line_width(1);
      cnv.stroke_round_rect(bounds, 6);

      // A fixed size badge, hard right and hard top inside the margin.
      auto slot = rect{0, 0, extent{64, 24}};
      auto badge = align(slot, bounds.inset(10), 1.0f, 0.0f);

      // Only the part of it that is on screen gets painted.
      if (intersects(badge, clip))
      {
         auto visible = intersection(badge, clip);
         cnv.fill_style(colors::medium_violet_red);
         cnv.fill_round_rect(visible, 4);
      }
   }
}

TEST_CASE("page examples: rect", "[page-examples]")
{
   float const w = 560, h = 180;
   image img{w, h, 2};
   {
      offscreen_image ctx{img};
      canvas cnv{ctx.context()};
      cnv.fill_style(colors::white);
      cnv.fill_rect(0, 0, w, h);
      cnv.translate(120, 10);

      rect_example(cnv);

      // Scaffolding, not part of the example: where clip runs, so the
      // reader can see what cut the badge.
      {
         auto s = cnv.new_state();
         cnv.line_width(1.25);
         cnv.stroke_style(rgba(229, 57, 53, 255));
         cnv.move_to(282, -10);
         cnv.line_to(282, 170);
         cnv.stroke();
      }
   }
   img.save_png(get_results_path() + "rect_example.png");

#if !defined(ARTIST_RECORDING)
   // bounds.inset(10) is 30, 30 to 310, 130, so a 64 by 24 slot aligned
   // right and top lands at 246, 30 to 310, 54. The clip cuts it at 282,
   // so under the shift 366 to 402 by 40 to 64 is painted and the rest of
   // the slot is not.
   auto badge = at(img, 380, 52);
   CHECK(badge[0] > 150);                 // the badge is magenta
   CHECK(badge[1] < 100);
   CHECK(at(img, 300, 52)[1] > 200);      // left of the badge, only card
   CHECK(at(img, 420, 52)[1] > 200);      // past the clip, only card
   CHECK(blank(at(img, 200, 5)));         // above the card, nothing
#endif
}

///////////////////////////////////////////////////////////////////////////////
// path.adoc

namespace
{
   void path_example(canvas& cnv)
   {
      // Built once, outside the draw function
      auto arrow = path{"M 0 0 L 40 20 L 0 40 L 10 20 Z"};

      // ... in the draw function
      cnv.fill_style(colors::navy_blue);
      for (int i = 0; i != 3; ++i)
      {
         auto s = cnv.new_state();
         cnv.translate(40.0f + i * 70, 30);
         cnv.add_path(arrow);
         cnv.fill();
      }
   }
}

TEST_CASE("page examples: path", "[page-examples]")
{
   float const w = 560, h = 100;
   image img{w, h, 2};
   {
      offscreen_image ctx{img};
      canvas cnv{ctx.context()};
      cnv.fill_style(colors::white);
      cnv.fill_rect(0, 0, w, h);
      cnv.translate(160, 0);
      path_example(cnv);
   }
   img.save_png(get_results_path() + "path_example.png");

#if !defined(ARTIST_RECORDING)
   // The three arrows sit at 200, 270 and 340, each 40 wide and 40 tall
   // from y 30. The notch at the back of the arrow is at x + 10.
   for (int i = 0; i != 3; ++i)
   {
      auto x = 200 + i * 70;
      CHECK(at(img, x + 15, 50)[2] > 100);   // the body, navy blue
      CHECK(blank(at(img, x + 4, 50)));      // inside the notch, blank
      CHECK(blank(at(img, x + 35, 35)));     // outside the point
   }
#endif
}
