/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]

   Asserts the claims of docs/modules/ROOT/pages/text_run.adoc. Each case
   names the page section it comes from. Behaviour under review is pinned in
   its own "current behaviour" case at the bottom; it records what the
   library does today, not what it should do.
=============================================================================*/
#include "test_support.hpp"
#include <functional>
#include <type_traits>

namespace fs = cycfi::fs;

namespace
{
   // Ink of a white-backed render, read through save_png and a reload so
   // that the pixels are 32 bit on every backend.
   struct ink_map
   {
      int w = 0, h = 0;
      std::vector<bool> ink;

      bool at(int x, int y) const { return ink[y * w + x]; }

      int top() const
      {
         for (int y = 0; y != h; ++y)
            for (int x = 0; x != w; ++x)
               if (at(x, y))
                  return y;
         return -1;
      }

      int bottom() const
      {
         for (int y = h - 1; y >= 0; --y)
            for (int x = 0; x != w; ++x)
               if (at(x, y))
                  return y;
         return -1;
      }

      int left() const
      {
         for (int x = 0; x != w; ++x)
            for (int y = 0; y != h; ++y)
               if (at(x, y))
                  return x;
         return -1;
      }

      int right(int y0, int y1) const
      {
         int r = -1;
         for (int y = y0; y < y1; ++y)
            for (int x = 0; x != w; ++x)
               if (at(x, y))
                  r = std::max(r, x);
         return r;
      }
   };

   ink_map render(int w, int h, std::function<void(canvas&)> f)
   {
      image img{float(w), float(h)};
      {
         offscreen_image ctx{img};
         canvas cnv{ctx.context()};
         cnv.fill_style(colors::white);
         cnv.fill_rect(0, 0, float(w), float(h));
         f(cnv);
      }
      static int n = 0;
      auto path = get_results_path() + "text_run_test_" + std::to_string(n++) + ".png";
      img.save_png(path);
      image loaded{fs::path{path}};
      auto p = reinterpret_cast<std::uint8_t const*>(loaded.pixels());
      ink_map m;
      m.w = int(loaded.bitmap_size().x);
      m.h = int(loaded.bitmap_size().y);
      m.ink.resize(m.w * m.h);
      for (int i = 0; i != m.w * m.h; ++i)
         m.ink[i] = p[4*i] < 128 || p[4*i+1] < 128 || p[4*i+2] < 128;
      return m;
   }

   auto const fd40 = font_descr{"Open Sans", 40};
   auto const words = std::string{"one two three four five six"};

   // The distinct baseline y of each line, in order.
   std::vector<float> baselines(text_run const& r)
   {
      std::vector<float> ys;
      for (std::size_t i = 0; i <= r.text().size(); ++i)
      {
         auto y = r.caret_point(i).y;
         if (ys.empty() || y > ys.back())
            ys.push_back(y);
      }
      return ys;
   }

   // The first code point index on the line whose baseline is y.
   std::size_t line_start(text_run const& r, float y)
   {
      for (std::size_t i = 0; i <= r.text().size(); ++i)
         if (r.caret_point(i).y == y)
            return i;
      return text_run::npos;
   }
}

TEST_CASE("text_run: Constructors", "[text_run]")
{
   static_assert(!std::is_copy_constructible_v<text_run>);
   static_assert(!std::is_copy_assignable_v<text_run>);
   static_assert(std::is_nothrow_move_constructible_v<text_run>);
   static_assert(!std::is_move_assignable_v<text_run>);
   static_assert(text_run::npos == std::size_t(-1));

   // UTF-8 is converted: indices are code points, not bytes.
   text_run a{fd40, "h\xc3\xa9llo"};
   CHECK(a.text().size() == 5);
   CHECK(a.text()[1] == U'\u00e9');   // é

   text_run b{fd40, std::u32string_view{U"héllo"}};
   CHECK(b.text() == a.text());

   text_run c{std::move(b)};
   CHECK(c.text() == a.text());
}

TEST_CASE("text_run: Text", "[text_run]")
{
   // No lines until the run is flowed, and replacing the text discards them.
   text_run r{fd40, "Hello"};
   CHECK(r.num_lines() == 0);
   CHECK(r.caret_point(0) == point{0, 0});
   CHECK(r.caret_index(5, 5) == 0);

   r.flow(500);
   CHECK(r.num_lines() == 1);

   r.text("Hello there");
   CHECK(r.text().size() == 11);
   CHECK(r.num_lines() == 0);

   r.flow(500);
   CHECK(r.num_lines() == 1);
}

TEST_CASE("text_run: Layout", "[text_run]")
{
   // flow(w): lines are the font's line_height() apart, from 0.
   {
      auto lh = font{fd40}.line_height();
      text_run r{fd40, words};
      r.flow(150);
      REQUIRE(r.num_lines() >= 3);
      auto ys = baselines(r);
      CHECK(ys.size() == r.num_lines());
      for (std::size_t k = 0; k != ys.size(); ++k)
         CHECK(ys[k] == Approx(k * lh).margin(0.01));
      CHECK(r.caret_point(0).x == 0);
   }

   // flow(glf, fi): glf is asked for each line with its baseline y; the
   // line starts at the offset glf returns; the last baseline moves down by
   // last_line_height - line_height.
   {
      text_run r{fd40, words};
      std::vector<float> asked;
      r.flow(
         [&](float y)
         {
            asked.push_back(y);
            return text_run::line_info{30, 150};
         }
       , {false, 50, 70}
      );
      auto n = r.num_lines();
      REQUIRE(n >= 3);
      REQUIRE(asked.size() >= n);
      for (std::size_t k = 0; k != n; ++k)
         CHECK(asked[k] == Approx(k * 50.0f));

      auto ys = baselines(r);
      REQUIRE(ys.size() == n);
      for (std::size_t k = 0; k + 1 < n; ++k)
         CHECK(ys[k] == Approx(k * 50.0f));
      CHECK(ys.back() == Approx((n - 1) * 50.0f + 20.0f));
      for (auto y : ys)
         CHECK(r.caret_point(line_start(r, y)).x == Approx(30.0f));
   }

   // Justification fills the width; the last line is left ragged.
   {
      std::string txt =
         "the quick brown fox jumps over the lazy dog and keeps running far away";
      auto ink = [&](bool justify)
      {
         return render(600, 200, [&](canvas& cnv)
         {
            text_run r{font_descr{"Open Sans", 20}, txt};
            r.flow(400, justify);
            REQUIRE(r.num_lines() == 2);
            r.draw(cnv, {10, 40});
         });
      };
      auto ragged = ink(false);
      auto justified = ink(true);
      CHECK(ragged.right(20, 45) < 400);
      CHECK(std::abs(justified.right(20, 45) - 410) <= 3);
      CHECK(justified.right(47, 72) == ragged.right(47, 72));
   }
}

TEST_CASE("text_run: Drawing", "[text_run]")
{
   // p is the left end of the first line's baseline. "Hxx" has no
   // descenders, so its ink ends at the baseline.
   auto ink = render(300, 200, [&](canvas& cnv)
   {
      text_run r{fd40, "Hxx"};
      r.flow(280);
      r.draw(cnv, {10, 100});
   });
   CHECK(ink.bottom() >= 97);
   CHECK(ink.bottom() <= 100);
   CHECK(ink.top() < 80);
   CHECK(ink.left() >= 10);
   CHECK(ink.left() <= 16);
}

TEST_CASE("text_run: Hit Testing", "[text_run]")
{
   text_run r{fd40, words};
   r.flow(150);
   auto ys = baselines(r);
   REQUIRE(ys.size() >= 3);
   auto s1 = line_start(r, ys[1]);
   auto s2 = line_start(r, ys[2]);

   // The line is the first one whose baseline is at or below y.
   CHECK(r.caret_index(0, -100) == 0);
   CHECK(r.caret_index(0, 0) == 0);
   CHECK(r.caret_index(0, 0.1f) == s1);
   CHECK(r.caret_index(0, ys[1]) == s1);
   CHECK(r.caret_index(0, ys[1] + 0.1f) == s2);
   CHECK(r.caret_index(0, ys.back() + 1) == text_run::npos);

   CHECK(r.caret_index(point{0, ys[1]}) == r.caret_index(0, ys[1]));

   // Past the end of a line that is not the last: the space that ended it.
   CHECK(r.caret_index(1e6f, 0) == s1 - 1);
   CHECK(r.text()[s1 - 1] == U' ');

   // Past the end of the last line: the end of the text.
   CHECK(r.caret_index(1e6f, ys.back()) == r.text().size());

   // An index at or past the end gives the end of the last line.
   CHECK(r.caret_point(r.text().size()).y == ys.back());
   CHECK(r.caret_point(r.text().size() + 10) == r.caret_point(r.text().size()));
}

TEST_CASE("text_run: Break Queries", "[text_run]")
{
   // The break is after code point i.
   text_run r{fd40, "ab cd\nef"};
   using b = text_run::break_enum;
   b const line[] = {
      text_run::no_break, text_run::no_break, text_run::allow_break,
      text_run::no_break, text_run::no_break, text_run::must_break,
      text_run::no_break, text_run::indeterminate
   };
   b const word[] = {
      text_run::no_break, text_run::allow_break, text_run::allow_break,
      text_run::no_break, text_run::allow_break, text_run::allow_break,
      text_run::no_break, text_run::allow_break
   };
   for (std::size_t i = 0; i != 8; ++i)
   {
      CHECK(r.line_break(i) == line[i]);
      CHECK(r.word_break(i) == word[i]);
   }
   CHECK(r.line_break(8) == text_run::indeterminate);
   CHECK(r.word_break(8) == text_run::indeterminate);

   // A text ending in a hard break: the last code point must break.
   text_run t{fd40, "ab\n"};
   CHECK(t.line_break(2) == text_run::must_break);
}

namespace
{
   color const note = rgba(93, 93, 93, 255);       // #5d5d5d
   color const accent = rgba(21, 101, 192, 255);   // #1565c0

   void arrowhead(canvas& cnv, point tip, point from)
   {
      auto dx = tip.x - from.x, dy = tip.y - from.y;
      auto len = std::sqrt(dx*dx + dy*dy);
      auto ux = dx / len, uy = dy / len;
      cnv.move_to(tip.x, tip.y);
      cnv.line_to(tip.x - 9*ux - 4*uy, tip.y - 9*uy + 4*ux);
      cnv.line_to(tip.x - 9*ux + 4*uy, tip.y - 9*uy - 4*ux);
      cnv.close_path();
      cnv.fill();
   }

   void dimension(canvas& cnv, point a, point b)
   {
      cnv.fill_style(note);
      cnv.stroke_style(note);
      cnv.line_width(1.25);
      cnv.begin_path();
      cnv.move_to(a);
      cnv.line_to(b);
      cnv.stroke();
      arrowhead(cnv, a, b);
      arrowhead(cnv, b, a);
   }

   // text_align(int), not text_align(text_halign): the latter ORs into the
   // current alignment, so it cannot return to left once right is set.
   void label(canvas& cnv, std::string_view s, point p, color c, int align)
   {
      cnv.font(font_descr{"Open Sans", 15});
      cnv.fill_style(c);
      cnv.text_align(align | canvas::baseline);
      cnv.fill_text(s, p);
      cnv.begin_path();
   }
}

TEST_CASE("text_run: Example", "[text_run]")
{
   // Page, Example. The block from `auto fd` to the caret is the page's
   // example verbatim. Its render, with annotations drawn over it from the
   // same run, is the page figure images/text_run/example.png, copied from
   // this test's output on the Cairo build.
   std::string text =
      "A text_run shapes a paragraph of text and breaks it into lines. The "
      "first line here is indented, the rest take the full width, and every "
      "line but the last is justified.";

   float const w = 560, h = 205;
   image img{2 * w, 2 * h};
   {
      offscreen_image ctx{img};
      canvas cnv{ctx.context()};
      cnv.scale(2);
      cnv.fill_style(colors::white);
      cnv.fill_rect(0, 0, w, h);

      auto mp = point{215, 80 + 2 * font{font_descr{"Open Sans", 20}}.line_height() - 7};

      // The page's example
      auto fd = font_descr{"Open Sans", 20};
      auto fm = font{fd}.metrics();
      auto lh = font{fd}.line_height();

      auto para = text_run{fd, text};
      para.flow(
         [](float y)
         {
            return y == 0
               ? text_run::line_info{30, 310}
               : text_run::line_info{0, 340};
         }
       , {true, lh, lh}
      );

      auto origin = point{60, 80};
      para.draw(cnv, origin);

      auto i = para.caret_index(mp.x - origin.x, mp.y - origin.y);
      if (i != text_run::npos)
      {
         auto cp = para.caret_point(i);
         cnv.stroke_style(colors::navy);
         cnv.move_to(origin.x + cp.x, origin.y + cp.y - fm.ascent);
         cnv.line_to(origin.x + cp.x, origin.y + cp.y + fm.descent);
         cnv.stroke();
      }
      // End of the page's example

      // The caret lands on the line whose band holds mp.y.
      REQUIRE(i != text_run::npos);
      auto ys = baselines(para);
      REQUIRE(ys.size() >= 4);
      CHECK(para.caret_point(i).y == ys[2]);

      // Only the first line is indented.
      CHECK(para.caret_point(0).x == Approx(30.0f));
      CHECK(para.caret_point(line_start(para, ys[1])).x == Approx(0.0f));

      // The text is chosen so that every line but the last fills more than
      // 90% of its width and is justified: each ends at x 340.
      for (std::size_t k = 0; k + 1 < ys.size(); ++k)
      {
         auto end = para.caret_index(1e6f, ys[k]);
         CHECK(para.caret_point(end).x == Approx(340.0f).margin(0.5));
      }

      // Annotations, from the same run.
      auto x0 = origin.x, y0 = origin.y;
      auto right = x0 + 340;

      cnv.fill_style(accent.opacity(0.12));
      cnv.fill_rect(x0, y0 + ys[1], 340, ys[2] - ys[1]);
      label(cnv, "caret_index band", {right + 14, y0 + (ys[1] + ys[2]) / 2 + 5},
         note, canvas::left);

      cnv.stroke_style(rgba(176, 176, 176, 255));
      cnv.line_width(1);
      for (std::size_t k = 0; k != ys.size(); ++k)
      {
         cnv.begin_path();
         cnv.move_to(x0 + (k == 0 ? 30 : 0), y0 + ys[k]);
         cnv.line_to(right, y0 + ys[k]);
         cnv.stroke();
      }

      cnv.fill_style(colors::black);
      cnv.begin_path();
      cnv.add_circle(x0, y0, 3.5);
      cnv.fill();
      label(cnv, "origin", {x0 - 9, y0 + 5}, colors::black, canvas::right);

      dimension(cnv, {right + 8, y0 + ys[0]}, {right + 8, y0 + ys[1]});
      label(cnv, "fi.line_height", {right + 14, y0 + (ys[0] + ys[1]) / 2 + 5},
         accent, canvas::left);

      // Offset and width of the first line, above it.
      auto yd = y0 - fm.ascent - 10;
      cnv.stroke_style(note);
      cnv.line_width(1.25);
      for (auto x : {x0, x0 + 30, right})
      {
         cnv.begin_path();
         cnv.move_to(x, yd - 8);
         cnv.line_to(x, yd + 8);
         cnv.stroke();
      }
      dimension(cnv, {x0, yd}, {x0 + 30, yd});
      dimension(cnv, {x0 + 30, yd}, {right, yd});
      label(cnv, "li.offset", {x0 + 15, yd - 13}, accent, canvas::center);
      label(cnv, "li.width", {(x0 + 30 + right) / 2, yd - 13}, accent, canvas::center);
   }
   img.save_png(get_results_path() + "text_run_example.png");
}

///////////////////////////////////////////////////////////////////////////////
// Current behaviour, under review. Records what the library does today so
// that a change is visible; it does not assert the behaviour is correct.
///////////////////////////////////////////////////////////////////////////////

TEST_CASE("text_run current behaviour: Quartz 2D caret indices", "[text_run]")
{
   // Three characters outside the Basic Multilingual Plane, then 'x': four
   // code points, seven UTF-16 code units.
   text_run r{fd40, std::u32string_view{U"\U0001F600\U0001F600\U0001F600x"}};
   r.flow(1000);
   REQUIRE(r.text().size() == 4);
#if defined(ARTIST_QUARTZ_2D)
   // Quartz 2D counts UTF-16 code units: past the end is 7, not 4.
   CHECK(r.caret_index(1e6f, 0) == 7);
#else
   CHECK(r.caret_index(1e6f, 0) == 4);
#endif
}
