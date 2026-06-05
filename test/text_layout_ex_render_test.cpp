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
