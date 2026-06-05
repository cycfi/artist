/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]

   Graphics-backed tests for text_layout_ex (elements #370): with real
   text_layout shaping, a multi-paragraph text_layout_ex must be geometrically
   equivalent to a single text_layout of the same text (which already handles
   hard '\n' breaks). This verifies the per-paragraph stitching — vertical
   offsets and caret index<->point mapping — against ground truth.
=============================================================================*/
#include "test_support.hpp"
#include <artist/text_layout_ex.hpp>

namespace
{
   // Compare text_layout_ex against a single text_layout of the same text.
   void check_equivalent(std::u32string_view text, float width)
   {
      text_layout doc{font_descr{"Open Sans", 14}, text};
      doc.flow(width);

      auto ex = make_text_layout_ex(font_descr{"Open Sans", 14}, text);
      ex.flow(width);

      INFO("text=\"" << cycfi::to_utf8(text) << "\" width=" << width);

      // Same number of laid-out lines.
      CHECK(ex.num_lines() == doc.num_lines());

      // Same caret geometry for every character index.
      for (std::size_t i = 0; i <= text.size(); ++i)
      {
         auto a = doc.caret_point(i);
         auto b = ex.caret_point(i);
         INFO("caret index " << i);
         CHECK(b.x == Approx(a.x).margin(0.01));
         CHECK(b.y == Approx(a.y).margin(0.01));
      }

      // For text without hard breaks, the whole document is one paragraph, so
      // word/line break classification must match the single text_layout
      // exactly at every index.
      if (text.find(U'\n') == std::u32string_view::npos)
      {
         for (std::size_t i = 0; i <= text.size(); ++i)
         {
            INFO("break index " << i);
            CHECK(ex.word_break(i) == doc.word_break(i));
            CHECK(ex.line_break(i) == doc.line_break(i));
         }
      }
   }
}

TEST_CASE("text_layout_ex matches text_layout (wide, no wrap)")
{
   check_equivalent(U"Hello, World", 1000);
   check_equivalent(U"one\ntwo\nthree", 1000);
   check_equivalent(U"alpha\n\nbeta", 1000);     // empty middle paragraph
   check_equivalent(U"trailing newline\n", 1000); // trailing empty paragraph
}

TEST_CASE("text_layout_ex matches text_layout (narrow, wrapping)")
{
   // Wrapping happens within a paragraph; '\n' starts a new paragraph. Both
   // engines must produce identical line structure and caret geometry.
   auto const para =
      U"the quick brown fox jumps over the lazy dog and keeps running along";
   check_equivalent(para, 120);
   check_equivalent(std::u32string(para) + U"\n" + std::u32string(para), 120);
}

TEST_CASE("text_layout_ex line_break marks paragraph boundaries")
{
   auto ex = make_text_layout_ex(font_descr{"Open Sans", 14}, U"one\ntwo\nthree");
   ex.flow(1000);

   // Paragraph starts (after each hard '\n') are mandatory line breaks; index 0
   // is the document start and is not classified as a break here.
   CHECK(ex.line_break(0) != text_layout::must_break);
   CHECK(ex.line_break(4) == text_layout::must_break);   // start of "two"
   CHECK(ex.line_break(8) == text_layout::must_break);   // start of "three"

   // A position inside a paragraph is not a mandatory break.
   CHECK(ex.line_break(1) != text_layout::must_break);
   CHECK(ex.line_break(5) != text_layout::must_break);
}

TEST_CASE("text_layout_ex caret_index hits the clicked row")
{
   // A click anywhere in a row's body must resolve to a caret on THAT row, not
   // the row/paragraph above or below. This is the multi-paragraph hit-test the
   // Elements editor relies on; blank paragraphs (their own one-line layout)
   // are the tricky case.
   auto const f = font_descr{"Open Sans", 14};
   auto const m = font{f}.metrics();
   float const line_height = m.ascent + m.descent + m.leading;

   std::u32string const text =
      U"the quick brown fox jumps over the lazy dog and runs on and on"
      U"\n\n"                                    // blank paragraph
      U"a second paragraph that also wraps across several lines when narrow"
      U"\nshort line";

   auto ex = make_text_layout_ex(f, text);
   ex.flow(120);                                 // force wrapping

   // For every character index, click the vertical centre of its row at the
   // glyph's own x; the hit must land back on the same row (same row top y).
   for (std::size_t i = 0; i <= text.size(); ++i)
   {
      auto cp = ex.caret_point(i);               // row top, glyph x
      point click{cp.x, cp.y + line_height * 0.5f};
      auto hit = ex.caret_index(click);
      INFO("index " << i << " click=(" << click.x << "," << click.y << ")");
      CHECK(ex.caret_point(hit).y == Approx(cp.y));
   }

   // Explicit blank-line case: clicking the body of the text line just below
   // the blank paragraph must land in that text line, not the blank above.
   std::size_t para3 = text.find(U"a second");
   auto top = ex.caret_point(para3);
   auto hit = ex.caret_index(point{top.x + 1.0f, top.y + line_height * 0.5f});
   CHECK(ex.caret_point(hit).y == Approx(top.y));
}

TEST_CASE("text_layout_ex incremental edit equals full rebuild")
{
   // After an incremental edit, the engine must equal a freshly-built engine
   // over the resulting text (i.e. incrementality introduces no drift).
   auto ex = make_text_layout_ex(font_descr{"Open Sans", 14}, U"one\ntwo\nthree");
   ex.flow(1000);
   ex.insert(4, U"XYZ");          // "one\nXYZtwo\nthree"
   ex.insert(0, U"start\n");      // "start\none\nXYZtwo\nthree"
   ex.erase(0, 6);                // back to "one\nXYZtwo\nthree"

   std::u32string expected = U"one\nXYZtwo\nthree";
   CHECK(ex.text() == expected);

   text_layout doc{font_descr{"Open Sans", 14}, expected};
   doc.flow(1000);
   CHECK(ex.num_lines() == doc.num_lines());
   for (std::size_t i = 0; i <= expected.size(); ++i)
   {
      auto a = doc.caret_point(i);
      auto b = ex.caret_point(i);
      INFO("caret index " << i);
      CHECK(b.x == Approx(a.x).margin(0.01));
      CHECK(b.y == Approx(a.y).margin(0.01));
   }
}
