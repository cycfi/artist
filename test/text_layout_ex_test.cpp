/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]

   Unit test for the multi-paragraph stitching of basic_text_layout_ex
   (elements #370). Uses a non-graphical fake paragraph layout so the
   stitching logic (paragraph splice on edits, vertical offsets, caret
   index<->point mapping, incrementality) is tested without a backend.
=============================================================================*/
#include <artist/text_layout_ex.hpp>
#include <string>
#include <vector>
#include <iostream>

using cycfi::artist::basic_text_layout_ex;
using cycfi::artist::point;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { \
   std::cerr << "FAIL " << __LINE__ << ": " #cond "\n"; ++failures; } } while (0)

// A non-graphical stand-in for a paragraph's text_layout.
//   num_lines() = 1 + number of '\n' in the paragraph text
//   caret_point(i) = {i, 0}   (x = local index, y = 0 within the paragraph)
//   caret_index({x,y}) = clamp(round(x), 0, size)
struct fake_layout
{
   std::u32string txt;
   float          width = 0;
   bool           justified = false;

                  fake_layout() = default;
   explicit       fake_layout(std::u32string_view s) : txt(s) {}
                  fake_layout(fake_layout const&) = delete;   // mimic non_copyable
                  fake_layout(fake_layout&&) = default;
   fake_layout&   operator=(fake_layout&&) = default;

   void           flow(float w, bool j) { width = w; justified = j; }
   std::size_t    num_lines() const
                  {
                     std::size_t n = 1;
                     for (auto c : txt) if (c == U'\n') ++n;
                     return n;
                  }
   point          caret_point(std::size_t i) const { return {float(i), 0}; }
   std::size_t    caret_index(point p) const
                  {
                     long x = long(p.x + 0.5f);
                     if (x < 0) x = 0;
                     if (x > long(txt.size())) x = long(txt.size());
                     return std::size_t(x);
                  }
};

using ex_t = basic_text_layout_ex<fake_layout>;

int main()
{
   // --- build: one paragraph layout per paragraph; vertical stack ---
   {
      int builds = 0;
      auto make = [&](std::u32string_view s) { ++builds; return fake_layout{s}; };
      ex_t ex(make, 10.0f, U"aa\nbb\ncc");
      ex.flow(100);

      CHECK(builds == 3);                    // one build per paragraph
      CHECK(ex.paragraph_count() == 3);
      CHECK(ex.text() == U"aa\nbb\ncc");
      CHECK(ex.num_lines() == 5);            // 2 + 2 + 1
      CHECK(ex.height() == 50.0f);           // 5 lines * 10

      auto p0 = ex.caret_point(0);  CHECK(p0.x == 0 && p0.y == 0);   // para 0
      auto p3 = ex.caret_point(3);  CHECK(p3.x == 0 && p3.y == 20);  // para 1 start
      auto p7 = ex.caret_point(7);  CHECK(p7.x == 1 && p7.y == 40);  // inside para 2

      CHECK(ex.caret_index({1, 40}) == 7);   // point -> index round trip
      CHECK(ex.caret_index({0, 20}) == 3);
   }

   // --- typing inside a paragraph rebuilds only that paragraph ---
   {
      int builds = 0;
      auto make = [&](std::u32string_view s) { ++builds; return fake_layout{s}; };
      ex_t ex(make, 10.0f, U"aa\nbb\ncc");
      ex.flow(100);
      builds = 0;

      ex.insert(3, U"X");                    // "aa\nXbb\ncc" (into paragraph 1)
      CHECK(builds == 1);                    // ONLY paragraph 1 rebuilt
      CHECK(ex.paragraph_count() == 3);
      CHECK(ex.text() == U"aa\nXbb\ncc");
      CHECK(ex.num_lines() == 5);
   }

   // --- inserting a newline splits one paragraph into two ---
   {
      int builds = 0;
      auto make = [&](std::u32string_view s) { ++builds; return fake_layout{s}; };
      ex_t ex(make, 10.0f, U"aa\nbb\ncc");
      ex.flow(100);
      builds = 0;

      ex.insert(1, U"\n");                   // "a\na\nbb\ncc": para 0 -> two paras
      CHECK(builds == 2);                    // two new paragraphs built
      CHECK(ex.paragraph_count() == 4);
      CHECK(ex.text() == U"a\na\nbb\ncc");
      // offsets must be consistent after the split
      CHECK(ex.caret_point(0).y == 0);
      CHECK(ex.caret_point(2).y == 20);      // second "a\n"
   }

   // --- erasing a newline merges two paragraphs into one ---
   {
      int builds = 0;
      auto make = [&](std::u32string_view s) { ++builds; return fake_layout{s}; };
      ex_t ex(make, 10.0f, U"aa\nbb\ncc");
      ex.flow(100);
      builds = 0;

      ex.erase(2, 1);                        // remove first '\n' -> "aabb\ncc"
      CHECK(builds == 1);                    // one merged paragraph built
      CHECK(ex.paragraph_count() == 2);
      CHECK(ex.text() == U"aabb\ncc");
      CHECK(ex.num_lines() == 3);            // "aabb\n"(2) + "cc"(1)
   }

   if (failures == 0) std::cout << "ALL PASS\n";
   else               std::cout << failures << " FAILURE(S)\n";
   return failures ? 1 : 0;
}
