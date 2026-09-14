/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]

   Asserts the claims of docs/modules/ROOT/pages/canvas/text.adoc. Each case
   names the page section it comes from. Every assertion holds on every
   backend, as the W3C canvas API specifies.

   Probes render onto a transparent image and compare where the ink lands,
   so they do not depend on a font's exact glyph shapes. "Painted" and
   "empty" are alpha above 200 and below 30.
=============================================================================*/
#include "test_support.hpp"
#include <functional>

namespace
{
   // pixels() is premultiplied B, G, R, A on every backend.
   int alpha_at(image const& img, int x, int y)
   {
      auto p = reinterpret_cast<std::uint8_t const*>(img.pixels());
      return p[4 * (y * int(img.bitmap_size().x) + x) + 3];
   }

   template <typename F>
   void render(image& img, F f)
   {
      offscreen_image ctx{img};
      canvas cnv{ctx.context()};
      cnv.font(font_descr{"Open Sans", 20});
      f(cnv);
   }

#if defined(ARTIST_RECORDING)
   // Where the last drawing call landed.
   bool same_ink(image const& a, image const& b)
   {
      return same_rect(
         recorded(a).commands().back().geometry,
         recorded(b).commands().back().geometry
      );
   }
#else
   // The box holding every half covered pixel.
   rect ink_bounds(image const& img)
   {
      img.pixels();
      int const w = img.bitmap_size().x;
      int const h = img.bitmap_size().y;
      int l = w, t = h, r = -1, b = -1;
      for (int y = 0; y != h; ++y)
      {
         for (int x = 0; x != w; ++x)
         {
            if (alpha_at(img, x, y) > 128)
            {
               l = std::min(l, x);
               t = std::min(t, y);
               r = std::max(r, x);
               b = std::max(b, y);
            }
         }
      }
      return {float(l), float(t), float(r + 1), float(b + 1)};
   }

   bool same_ink(image const& a, image const& b)
   {
      return ink_bounds(a) == ink_bounds(b);
   }
#endif

   using draw_fn = std::function<void(canvas&)>;

   // The same text drawn after `setup`, at a point clear of every edge.
   image aligned(draw_fn setup)
   {
      image img{200, 100, 1};
      render(img, [&](canvas& cnv)
      {
         setup(cnv);
         cnv.fill_text("MMMM", 100, 50);
      });
      return img;
   }
}

TEST_CASE("canvas text: text_align / text_baseline", "[text]")
{
   auto const left = aligned([](canvas& cnv) { cnv.text_align(canvas::left); });
   auto const center = aligned([](canvas& cnv) { cnv.text_align(canvas::center); });
   auto const top = aligned([](canvas& cnv) { cnv.text_baseline(canvas::top); });
   auto const bottom = aligned([](canvas& cnv) { cnv.text_baseline(canvas::bottom); });
   auto const plain = aligned([](canvas&) {});

   // The probe can tell the alignments apart.
   CHECK(!same_ink(left, center));
   CHECK(!same_ink(top, bottom));
   CHECK(!same_ink(top, plain));

   // The defaults are left and baseline.
   CHECK(same_ink(plain, left));
   CHECK(same_ink(plain, aligned([](canvas& cnv) { cnv.text_baseline(canvas::baseline); })));

   // Setting an alignment replaces the one set before.
   CHECK(same_ink(left, aligned([](canvas& cnv)
   {
      cnv.text_align(canvas::center);
      cnv.text_align(canvas::left);
   })));
   CHECK(same_ink(center, aligned([](canvas& cnv)
   {
      cnv.text_align(canvas::right);
      cnv.text_align(canvas::center);
   })));
   CHECK(same_ink(bottom, aligned([](canvas& cnv)
   {
      cnv.text_baseline(canvas::top);
      cnv.text_baseline(canvas::bottom);
   })));
   CHECK(same_ink(plain, aligned([](canvas& cnv)
   {
      cnv.text_baseline(canvas::middle);
      cnv.text_baseline(canvas::baseline);
   })));

   // The horizontal and vertical settings are independent.
   auto const right_top = aligned([](canvas& cnv)
   {
      cnv.text_align(canvas::right | canvas::top);
   });
   CHECK(same_ink(right_top, aligned([](canvas& cnv)
   {
      cnv.text_align(canvas::right);
      cnv.text_baseline(canvas::top);
   })));
   CHECK(same_ink(right_top, aligned([](canvas& cnv)
   {
      cnv.text_baseline(canvas::top);
      cnv.text_align(canvas::left);
      cnv.text_align(canvas::right);
   })));

   // The combined form sets both, so what it leaves out is the default.
   CHECK(same_ink(center, aligned([](canvas& cnv)
   {
      cnv.text_baseline(canvas::bottom);
      cnv.text_align(canvas::center | canvas::baseline);
   })));
}

TEST_CASE("canvas text: fill_text / stroke_text leave the current path", "[text]")
{
   color const blue = rgba(0, 0, 255, 255);

   auto gradient = []
   {
      canvas::linear_gradient gr{0, 0, 60, 0};
      gr.add_color_stop(0, rgba(0, 0, 255, 255));
      gr.add_color_stop(1, rgba(0, 0, 255, 255));
      return gr;
   };

   struct { char const* name; draw_fn draw; } const cases[] = {
      {"fill_text", [](canvas& cnv)
         {
            cnv.fill_text("Ag", 10, 60);
         }},
      {"fill_text with a gradient", [&](canvas& cnv)
         {
            cnv.fill_style(gradient());
            cnv.fill_text("Ag", 10, 60);
         }},
      {"fill_text with a shadow", [&](canvas& cnv)
         {
            cnv.shadow_style({2, 2}, 0, blue);
            cnv.fill_text("Ag", 10, 60);
         }},
      {"stroke_text", [](canvas& cnv)
         {
            cnv.stroke_text("Ag", 10, 60);
         }},
      {"stroke_text with a gradient", [&](canvas& cnv)
         {
            cnv.stroke_style(gradient());
            cnv.stroke_text("Ag", 10, 60);
         }}
   };

   for (auto const& c : cases)
   {
      INFO(c.name);
      image img{200, 100, 1};
      render(img, [&](canvas& cnv)
      {
         // A wide stroke, so stroking the rectangle would show outside it.
         cnv.line_width(6);
         cnv.add_rect(120, 20, 60, 60);
         c.draw(cnv);
         cnv.fill();
      });
#if defined(ARTIST_RECORDING)
      auto const& last = recorded(img).commands().back();
      CHECK(last.kind == recording::op::fill);
      CHECK(same_rect(last.geometry, {120, 20, 180, 80}));
#else
      CHECK(alpha_at(img, 150, 50) > 200);   // the rectangle is still filled
      CHECK(alpha_at(img, 118, 30) < 30);    // and was never stroked
#endif
   }
}
