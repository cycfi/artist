/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]

   Asserts the claims of docs/modules/ROOT/pages/canvas/text.adoc and draws
   its figures. Each case names the page section it comes from. Every
   assertion holds on every backend, as the W3C canvas API specifies.

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

namespace
{
   struct rgba8
   {
      int r, g, b, a;
   };

   rgba8 pixel_at(image const& img, int x, int y)
   {
      auto p = reinterpret_cast<std::uint8_t const*>(img.pixels());
      p += 4 * (y * int(img.bitmap_size().x) + x);
      return {p[2], p[1], p[0], p[3]};
   }

   // Where the text landed: the painted pixels, or on the recording backend
   // the line box the last operation recorded. Both move with the point.
   rect text_box(image const& img)
   {
#if defined(ARTIST_RECORDING)
      return recorded(img).commands().back().geometry;
#else
      return ink_bounds(img);
#endif
   }

   canvas::text_metrics measured(char const* utf8)
   {
      canvas::text_metrics m;
      image img{10, 10, 1};
      render(img, [&](canvas& cnv) { m = cnv.measure_text(utf8); });
      return m;
   }

   bool close_to(float a, float b, float tol = 1.5f)
   {
      return std::abs(a - b) <= tol;
   }
}

TEST_CASE("canvas text: Font", "[text]")
{
   canvas::text_metrics small, large, restored;
   image img{10, 10, 1};
   render(img, [&](canvas& cnv)
   {
      cnv.font(font_descr{"Open Sans", 20});
      small = cnv.measure_text("MMMM");
      {
         auto s = cnv.new_state();
         cnv.font(font_descr{"Open Sans", 40});
         large = cnv.measure_text("MMMM");
      }
      restored = cnv.measure_text("MMMM");
   });

   // Text is measured, and so drawn, in the font set.
   CHECK(close_to(large.size.x, 2 * small.size.x));
   CHECK(close_to(large.ascent, 2 * small.ascent));

   // The font is saved state.
   CHECK(restored.size.x == small.size.x);
   CHECK(restored.ascent == small.ascent);
}

TEST_CASE("canvas text: alignment is saved state", "[text]")
{
   auto const center = aligned([](canvas& cnv) { cnv.text_align(canvas::center); });
   CHECK(same_ink(center, aligned([](canvas& cnv)
   {
      cnv.text_align(canvas::center);
      {
         auto s = cnv.new_state();
         cnv.text_align(canvas::right);
         cnv.text_baseline(canvas::top);
      }
   })));
}

TEST_CASE("canvas text: Drawing", "[text]")
{
   color const red = rgba(255, 0, 0, 255);
   color const blue = rgba(0, 0, 255, 255);

   {
      // fill_text paints with the fill style, stroke_text with the stroke
      // style at the line width.
      image img{200, 100, 1};
      render(img, [&](canvas& cnv)
      {
         cnv.font(font_descr{"Open Sans", 60}.bold());
         cnv.fill_style(red);
         cnv.stroke_style(blue);
         cnv.line_width(4);
         cnv.fill_text("I", 20, 80);
         cnv.stroke_text("I", 120, 80);
      });
#if defined(ARTIST_RECORDING)
      REQUIRE(recorded(img).size() == 2);
      CHECK(recorded(img, 0).kind == recording::op::fill_text);
      CHECK(same_color(recorded(img, 0).paint, red));
      CHECK(recorded(img, 1).kind == recording::op::stroke_text);
      CHECK(same_color(recorded(img, 1).paint, blue));
      CHECK(recorded(img, 1).line_width == 4);
#else
      auto painted_in = [&](int x0, int x1, rgba8 want)
      {
         int hits = 0, wrong = 0;
         for (int y = 0; y != 100; ++y)
         {
            for (int x = x0; x != x1; ++x)
            {
               auto p = pixel_at(img, x, y);
               if (p.a > 250)
               {
                  ++hits;
                  if (std::abs(p.r - want.r) > 3 || std::abs(p.b - want.b) > 3)
                     ++wrong;
               }
            }
         }
         return hits > 20 && wrong == 0;
      };
      CHECK(painted_in(0, 100, {255, 0, 0, 255}));
      CHECK(painted_in(100, 200, {0, 0, 255, 255}));
#endif
   }

   {
      // With the default alignment, p is where the baseline starts.
      auto const box = text_box(aligned([](canvas&) {}));
      auto const m = measured("MMMM");
#if defined(ARTIST_RECORDING)
      CHECK(close_to(box.left, 100));
      CHECK(close_to(box.top, 50 - m.ascent));
      CHECK(close_to(box.bottom, 50 + m.descent));
#else
      CHECK(close_to(box.left, 100, 3));     // allow the glyph's side bearing
      CHECK(close_to(box.bottom, 50));       // M sits on the baseline
      CHECK(box.right <= 100 + m.size.x + 1);
#endif
   }

   {
      // Text follows the transform.
      image one{200, 100, 1};
      render(one, [](canvas& cnv) { cnv.fill_text("MM", 10, 50); });
      image two{200, 100, 1};
      render(two, [](canvas& cnv)
      {
         cnv.scale(2);
         cnv.fill_text("MM", 10, 25);
      });
      auto const a = text_box(one);
      auto const b = text_box(two);
      CHECK(close_to(b.width(), 2 * a.width(), 2));
      CHECK(close_to(b.height(), 2 * a.height(), 2));
      CHECK(close_to(b.left, 2 * a.left, 2));
   }
}

TEST_CASE("canvas text: where each alignment puts the point", "[text]")
{
   auto const m = measured("MMMM");
   auto const plain = text_box(aligned([](canvas&) {}));
   auto shift = [&](draw_fn setup)
   {
      auto const b = text_box(aligned(setup));
      return point{b.left - plain.left, b.top - plain.top};
   };

   auto s = shift([](canvas& cnv) { cnv.text_align(canvas::center); });
   CHECK(close_to(s.x, -m.size.x / 2));
   CHECK(close_to(s.y, 0));

   s = shift([](canvas& cnv) { cnv.text_align(canvas::right); });
   CHECK(close_to(s.x, -m.size.x));
   CHECK(close_to(s.y, 0));

   s = shift([](canvas& cnv) { cnv.text_baseline(canvas::top); });
   CHECK(close_to(s.x, 0));
   CHECK(close_to(s.y, m.ascent));

   s = shift([](canvas& cnv) { cnv.text_baseline(canvas::middle); });
   CHECK(close_to(s.y, (m.ascent - m.descent) / 2));

   s = shift([](canvas& cnv) { cnv.text_baseline(canvas::bottom); });
   CHECK(close_to(s.y, -m.descent));
}

TEST_CASE("canvas text: Measurement", "[text]")
{
   auto const a = measured("a");
   auto const ag = measured("Ag");

   CHECK(ag.ascent > 0);
   CHECK(ag.descent > 0);
   CHECK(ag.leading >= 0);

   // The ascent, descent and leading belong to the font.
   CHECK(a.ascent == ag.ascent);
   CHECK(a.descent == ag.descent);
   CHECK(a.leading == ag.leading);

   // size.x is the advance, trailing spaces included.
   CHECK(close_to(measured("MMMMMMMM").size.x, 2 * measured("MMMM").size.x, 1));
   CHECK(measured("M ").size.x > measured("M").size.x);
}

TEST_CASE("canvas text: Example", "[text]")
{
   image img{200, 100, 1};
   rect label_box;
   render(img, [&](canvas& cnv)
   {
      auto const pos = point{20, 20};

      // The page's Example.
      cnv.font(font_descr{"Open Sans", 16}.bold());
      auto m = cnv.measure_text("Cancel");

      auto box = rect{pos, extent{m.size.x + 24, m.ascent + m.descent + 12}};
      cnv.fill_style(colors::gray[30]);
      cnv.fill_round_rect(box, 6);

      cnv.fill_style(colors::white);
      cnv.text_align(canvas::center | canvas::middle);
      cnv.fill_text("Cancel", center_point(box));

      label_box = box;
   });

#if defined(ARTIST_RECORDING)
   auto const& last = recorded(img).commands().back();
   CHECK(last.kind == recording::op::fill_text);
   CHECK(close_to(center_point(last.geometry).x, center_point(label_box).x));
#else
   // The white label is centred across the box.
   int l = 200, r = -1;
   for (int y = 0; y != 100; ++y)
   {
      for (int x = 0; x != 200; ++x)
      {
         auto p = pixel_at(img, x, y);
         if (p.a > 250 && p.r > 200 && p.g > 200 && p.b > 200)
         {
            l = std::min(l, x);
            r = std::max(r, x + 1);
         }
      }
   }
   REQUIRE(r > l);
   CHECK(close_to((l + r) / 2.0f, center_point(label_box).x, 2));
   CHECK(l > label_box.left);
   CHECK(r < label_box.right);
#endif
}

namespace
{
   color const ink = rgba(26, 26, 26, 255);        // #1a1a1a
   color const ghost = rgba(176, 176, 176, 255);   // #b0b0b0
   color const accent = rgba(21, 101, 192, 255);   // #1565c0
   color const amber = rgba(255, 179, 0, 255);     // #ffb300

   void caption(canvas& cnv, char const* s, float x, float y)
   {
      auto st = cnv.new_state();
      cnv.font(font_descr{"Open Sans", 15});
      cnv.fill_style(ink);
      cnv.text_align(canvas::center | canvas::baseline);
      cnv.fill_text(s, x, y);
   }

   void anchor(canvas& cnv, float x, float y)
   {
      auto st = cnv.new_state();
      cnv.fill_style(accent);
      cnv.add_circle(x, y, 4);
      cnv.fill();
   }

   void guide(canvas& cnv, point a, point b)
   {
      auto st = cnv.new_state();
      cnv.stroke_style(ghost);
      cnv.line_width(1.25);
      cnv.move_to(a);
      cnv.line_to(b);
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

   auto const word_font = font_descr{"Open Sans", 60}.bold();
}

TEST_CASE("canvas text: fill and stroke figure", "[text]")
{
   // The page figure images/canvas/text_fill_stroke.png.
   figure(150, "canvas_text_fill_stroke.png", [](canvas& cnv)
   {
      cnv.font(word_font);
      cnv.text_align(canvas::center | canvas::baseline);

      cnv.fill_style(accent);
      cnv.fill_text("Text", 95, 90);

      cnv.stroke_style(accent);
      cnv.line_width(2);
      cnv.stroke_text("Text", 280, 90);

      cnv.fill_style(amber);
      cnv.fill_text("Text", 465, 90);
      cnv.stroke_style(accent);
      cnv.line_width(2);
      cnv.stroke_text("Text", 465, 90);

      caption(cnv, "fill_text", 95, 130);
      caption(cnv, "stroke_text", 280, 130);
      caption(cnv, "fill_text + stroke_text", 465, 130);
   });
}

TEST_CASE("canvas text: styled text figure", "[text]")
{
   // The page figure images/canvas/text_styles.png: text takes the same
   // paint and shadow styles as a path.
   figure(150, "canvas_text_styles.png", [](canvas& cnv)
   {
      cnv.font(word_font);
      cnv.text_align(canvas::center | canvas::baseline);
      auto half = cnv.measure_text("Text").size.x / 2;

      auto gradient = [&](float cx)
      {
         canvas::linear_gradient gr{cx - half, 0, cx + half, 0};
         gr.add_color_stop(0, accent);
         gr.add_color_stop(1, amber);
         return gr;
      };

      cnv.fill_style(gradient(95));
      cnv.fill_text("Text", 95, 90);

      cnv.stroke_style(gradient(280));
      cnv.line_width(3);
      cnv.stroke_text("Text", 280, 90);

      {
         auto st = cnv.new_state();
         cnv.shadow_style({4, 4}, 8, colors::black.opacity(0.45));
         cnv.fill_style(accent);
         cnv.fill_text("Text", 465, 90);
      }

      caption(cnv, "fill_style(gradient)", 95, 130);
      caption(cnv, "stroke_style(gradient)", 280, 130);
      caption(cnv, "shadow_style", 465, 130);
   });
}

TEST_CASE("canvas text: alignment figure", "[text]")
{
   // The page figure images/canvas/text_align.png: the dot is the point
   // given to fill_text, the grey guides run through it.
   figure(300, "canvas_text_align.png", [](canvas& cnv)
   {
      auto const size = 32.0f;
      cnv.font(font_descr{"Open Sans", size});
      cnv.fill_style(ink);

      // Horizontal alignment: one vertical guide per word.
      {
         struct { char const* word; char const* name; int align; float x; }
         const rows[] = {
            {"left", "canvas::left", canvas::left, 100},
            {"center", "canvas::center", canvas::center, 280},
            {"right", "canvas::right", canvas::right, 460}
         };
         float const y = 70;
         for (auto const& r : rows)
         {
            guide(cnv, {r.x, 25}, {r.x, 90});
            cnv.text_align(r.align | canvas::baseline);
            cnv.fill_text(r.word, r.x, y);
            anchor(cnv, r.x, y);
            caption(cnv, r.name, r.x, 115);
         }
      }

      // Vertical alignment: one horizontal guide through every word.
      {
         struct { char const* word; char const* name; int align; float x; }
         const cols[] = {
            {"top", "canvas::top", canvas::top, 80},
            {"middle", "canvas::middle", canvas::middle, 205},
            {"baseline", "canvas::baseline", canvas::baseline, 340},
            {"bottom", "canvas::bottom", canvas::bottom, 475}
         };
         float const y = 205;
         guide(cnv, {20, y}, {540, y});
         for (auto const& c : cols)
         {
            cnv.text_align(canvas::center | c.align);
            cnv.fill_text(c.word, c.x, y);
            anchor(cnv, c.x, y);
            caption(cnv, c.name, c.x, 275);
         }
      }
   });
}
