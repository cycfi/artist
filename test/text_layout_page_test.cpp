/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]

   Asserts the claims of docs/modules/ROOT/pages/text_layout.adoc. Each case
   names the page section it comes from.
=============================================================================*/
#include "test_support.hpp"
#include <artist/text_layout.hpp>
#include <type_traits>

namespace
{
   auto const fd = font_descr{"Open Sans", 20};

   float line_height()
   {
      return font{fd}.line_height();
   }

   std::u32string const long_para =
      U"the quick brown fox jumps over the lazy dog and keeps running along "
      U"the river bank until the sun goes down";
}

TEST_CASE("text_layout: Construction", "[text_layout]")
{
   static_assert(!std::is_copy_constructible_v<text_layout>);
   static_assert(std::is_nothrow_move_constructible_v<text_layout>);
   static_assert(std::is_nothrow_move_assignable_v<text_layout>);
   static_assert(text_layout::npos == std::size_t(-1));
   static_assert(std::is_same_v<text_layout::break_enum, text_run::break_enum>);

   // Paragraphs are split at '\n'; a trailing '\n' makes a final empty one.
   auto ex = make_text_layout(fd, U"one\ntwo\n");
   CHECK(ex.text() == U"one\ntwo\n");
   CHECK(ex.size() == 8);
   CHECK(ex.paragraph_count() == 3);

   // Before the first flow every paragraph counts as one line.
   CHECK(ex.num_lines() == 3);
   CHECK(ex.height() == Approx(3 * line_height()));

   // make_text_layout's line height is the font's.
   auto moved = std::move(ex);
   CHECK(moved.size() == 8);
}

TEST_CASE("text_layout: Layout", "[text_layout]")
{
   auto lh = line_height();
   auto ex = make_text_layout(fd, long_para + U"\n\n" + long_para);
   ex.flow(200);

   // Each paragraph wraps on its own; an empty paragraph is one line.
   CHECK(ex.paragraph_count() == 3);
   CHECK(ex.paragraph_lines(0) > 1);
   CHECK(ex.paragraph_lines(1) == 1);
   CHECK(ex.paragraph_lines(2) == ex.paragraph_lines(0));
   CHECK(ex.num_lines() ==
      ex.paragraph_lines(0) + ex.paragraph_lines(1) + ex.paragraph_lines(2));
   CHECK(ex.height() == Approx(ex.num_lines() * lh));

   // set_text lays the new text out at the current width at once.
   ex.set_text(long_para);
   CHECK(ex.paragraph_count() == 1);
   CHECK(ex.num_lines() > 1);

   auto fresh = make_text_layout(fd, long_para);
   fresh.flow(200);
   CHECK(ex.num_lines() == fresh.num_lines());
}

TEST_CASE("text_layout: Editing", "[text_layout]")
{
   auto ex = make_text_layout(fd, U"one two");
   ex.flow(1000);

   ex.insert(3, U"\nXYZ");                    // splits the paragraph
   CHECK(ex.text() == U"one\nXYZ two");
   CHECK(ex.paragraph_count() == 2);

   ex.insert(1000, U"!");                     // past the end: appended
   CHECK(ex.text() == U"one\nXYZ two!");

   ex.insert(0, U"");                         // empty: no change
   CHECK(ex.text() == U"one\nXYZ two!");

   ex.erase(3, 1);                            // the '\n': paragraphs merge
   CHECK(ex.text() == U"oneXYZ two!");
   CHECK(ex.paragraph_count() == 1);

   ex.erase(6, 1000);                         // length clamps to the end
   CHECK(ex.text() == U"oneXYZ");

   ex.erase(100, 1);                          // past the end: no change
   CHECK(ex.text() == U"oneXYZ");

   ex.replace(3, 3, U" and\nmore");
   CHECK(ex.text() == U"one and\nmore");
   CHECK(ex.paragraph_count() == 2);

   // Edits are laid out at the current width without another flow.
   auto lh = line_height();
   ex.flow(200);
   ex.insert(0, long_para + U" ");
   auto fresh = make_text_layout(fd, ex.text());
   fresh.flow(200);
   CHECK(ex.num_lines() == fresh.num_lines());
   CHECK(ex.height() == Approx(fresh.num_lines() * lh));
}

TEST_CASE("text_layout: Hit Testing", "[text_layout]")
{
   auto lh = line_height();
   auto ex = make_text_layout(fd, U"one\ntwo\nthree");
   ex.flow(1000);

   // caret_point gives the top of the caret's line: line k at k * lh.
   CHECK(ex.caret_point(0) == point{0, 0});
   CHECK(ex.caret_point(4).y == Approx(lh));
   CHECK(ex.caret_point(8).y == Approx(2 * lh));
   CHECK(ex.caret_point(4).x == Approx(0));

   // caret_index takes the same coordinates: line k for y in
   // [k * lh, (k + 1) * lh).
   CHECK(ex.caret_index(0, 0.1f) == 0);
   CHECK(ex.caret_index(0, lh - 0.1f) == 0);
   CHECK(ex.caret_index(0, lh + 0.1f) == 4);
   CHECK(ex.caret_index(0, 2 * lh + 0.1f) == 8);
   CHECK(ex.caret_index(point{0, lh + 0.1f}) == ex.caret_index(0, lh + 0.1f));

   // Points above and below the text clamp to the first and last lines.
   CHECK(ex.caret_index(0, -50) == 0);
   CHECK(ex.caret_index(0, 100 * lh) == 8);
   CHECK(ex.caret_index(1e6f, 100 * lh) == ex.size());

   // An index at or past the end gives the end of the last line.
   CHECK(ex.caret_point(ex.size() + 5) == ex.caret_point(ex.size()));
}

TEST_CASE("text_layout: Drawing", "[text_layout]")
{
   // p is the left end of the first line's baseline, as for text_run: "Hxx"
   // has no descenders, so its ink ends at the baseline.
   image img{300, 200};
   {
      offscreen_image ctx{img};
      canvas cnv{ctx.context()};
      cnv.fill_style(colors::white);
      cnv.fill_rect(0, 0, 300, 200);
      auto ex = make_text_layout(fd, U"Hxx");
      ex.flow(280);
      ex.draw(cnv, {10, 100});
   }
   auto p = reinterpret_cast<std::uint8_t const*>(img.pixels());
   int w = int(img.bitmap_size().x), h = int(img.bitmap_size().y);
   int bottom = -1;
   for (int y = 0; y != h; ++y)
      for (int x = 0; x != w; ++x)
      {
         auto q = p + 4 * (y * w + x);
         if (q[0] < 128 || q[1] < 128 || q[2] < 128)
            bottom = y;
      }
   CHECK(bottom >= 97);
   CHECK(bottom <= 100);
}

TEST_CASE("text_layout: Break Queries", "[text_layout]")
{
   auto ex = make_text_layout(fd, U"ab cd\nef");
   ex.flow(1000);

   // A '\n' reports a required break after itself.
   CHECK(ex.line_break(5) == text_run::must_break);
   CHECK(ex.line_break(2) == text_run::allow_break);
   CHECK(ex.line_break(0) == text_run::no_break);
   CHECK(ex.word_break(1) == text_run::allow_break);
   CHECK(ex.word_break(0) == text_run::no_break);
}

namespace
{
   color const note = rgba(93, 93, 93, 255);       // #5d5d5d
   color const accent = rgba(21, 101, 192, 255);   // #1565c0

   void dimension(canvas& cnv, point a, point b)
   {
      auto head = [&](point tip, point from)
      {
         auto dx = tip.x - from.x, dy = tip.y - from.y;
         auto len = std::sqrt(dx*dx + dy*dy);
         auto ux = dx / len, uy = dy / len;
         cnv.begin_path();
         cnv.move_to(tip.x, tip.y);
         cnv.line_to(tip.x - 9*ux - 4*uy, tip.y - 9*uy + 4*ux);
         cnv.line_to(tip.x - 9*ux + 4*uy, tip.y - 9*uy - 4*ux);
         cnv.close_path();
         cnv.fill();
      };
      cnv.fill_style(note);
      cnv.stroke_style(note);
      cnv.line_width(1.25);
      cnv.begin_path();
      cnv.move_to(a);
      cnv.line_to(b);
      cnv.stroke();
      head(a, b);
      head(b, a);
   }

   void label(canvas& cnv, std::string_view s, point p, color c, int align)
   {
      cnv.font(font_descr{"Open Sans", 15});
      cnv.fill_style(c);
      cnv.text_align(align | canvas::baseline);
      cnv.fill_text(s, p);
      cnv.begin_path();
   }
}

TEST_CASE("text_layout: Example", "[text_layout]")
{
   // Page, Example. The block from `auto fd` to the caret is the page's
   // example verbatim. Its render, annotated from the same layout, is the
   // page figure images/text_layout/example.png, copied from this test's
   // output on the Cairo build.
   std::u32string text =
      U"A text_layout splits its text into paragraphs at each line feed "
      U"and lays each one out with its own text_run.\n"
      U"An edit shapes again only the paragraphs it touches.";

   float const w = 560, h = 260;
   image img{w, h, 2};
   {
      offscreen_image ctx{img};
      canvas cnv{ctx.context()};
      cnv.fill_style(colors::white);
      cnv.fill_rect(0, 0, w, h);

      auto mp = point{240, 30 + 2.5f * font{font_descr{"Open Sans", 20}}.line_height()};

      // The page's example
      auto fd = font_descr{"Open Sans", 20};
      auto fm = font{fd}.metrics();
      auto lh = font{fd}.line_height();

      auto doc = make_text_layout(fd, text);
      doc.flow(340);
      doc.insert(doc.size(), U"\nThis paragraph was added after the flow.");

      auto top_left = point{90, 30};
      doc.draw(cnv, {top_left.x, top_left.y + fm.ascent});

      auto i = doc.caret_index(mp.x - top_left.x, mp.y - top_left.y);
      auto cp = doc.caret_point(i);
      cnv.stroke_style(colors::navy);
      cnv.move_to(top_left.x + cp.x, top_left.y + cp.y);
      cnv.line_to(top_left.x + cp.x, top_left.y + cp.y + lh);
      cnv.stroke();
      // End of the page's example

      // The click is in line 2's box, and the caret is drawn there.
      CHECK(cp.y == Approx(2 * lh));
      CHECK(doc.paragraph_count() == 3);

      // Annotations, from the same layout: alternating line boxes, the
      // document's top-left and the draw point p.
      auto x0 = top_left.x, y0 = top_left.y;
      auto right = x0 + 340;
      for (std::size_t k = 0; k != doc.num_lines(); ++k)
      {
         cnv.fill_style(accent.opacity(k % 2 ? 0.14f : 0.06f));
         cnv.fill_rect(x0, float(y0 + k * lh), 340, float(lh));
      }

      cnv.fill_style(colors::black);
      cnv.begin_path();
      cnv.add_circle(x0, y0, 3.5);
      cnv.fill();
      label(cnv, "top-left", {x0 - 9, y0 + 5}, colors::black, canvas::right);

      cnv.begin_path();
      cnv.add_circle(x0, y0 + fm.ascent, 3.5);
      cnv.fill();
      label(cnv, "p", {x0 - 9, y0 + fm.ascent + 5}, colors::black, canvas::right);

      dimension(cnv, {right + 8, y0}, {right + 8, float(y0 + lh)});
      label(cnv, "line height", {right + 14, float(y0 + lh / 2 + 5)}, accent, canvas::left);
   }
   img.save_png(get_results_path() + "text_layout_example.png");
}
