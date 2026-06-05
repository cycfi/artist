/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]

   Focused per-edit fuzz for text_layout_ex incremental updates (elements #370).
   Narrow flow width forces paragraphs to wrap (multi-line), and after EVERY
   random edit the incrementally-updated engine is compared to a freshly built
   text_layout of the same text. Pinpoints incremental-update drift.
=============================================================================*/
#include "test_support.hpp"
#include <artist/text_layout_ex.hpp>
#include <iostream>
#include <random>
#include <string>

namespace
{
   auto const fnt = font_descr{"Open Sans", 13};
   constexpr float width = 120.0f;            // narrow -> wrapping
}

TEST_CASE("text_layout_ex incremental matches fresh build after each edit")
{
   std::u32string oracle =
      U"The quick brown fox jumps over the lazy dog and keeps on running.\n"
      U"Second paragraph is also fairly long so that it wraps onto lines.\n\n"
      U"Third paragraph.\nshort\n";
   auto ex = make_text_layout_ex(fnt, oracle);
   ex.flow(width);

   std::mt19937 rng(0xF0CA1);
   auto rnd_text = [&](std::size_t n)
   {
      std::u32string s;
      for (std::size_t i = 0; i < n; ++i)
      {
         unsigned r = rng() % 14;
         s.push_back(r == 0 ? U'\n' : char32_t(U'a' + (r % 26)));
      }
      return s;
   };

   for (int op = 0; op < 4000; ++op)
   {
      std::size_t sz = oracle.size();
      if ((rng() & 1) || sz == 0)
      {
         std::size_t pos = sz ? rng() % (sz + 1) : 0;
         auto t = rnd_text(1 + rng() % 8);
         ex.insert(pos, t);
         oracle.insert(pos, t);
      }
      else
      {
         std::size_t pos = rng() % sz;
         std::size_t len = 1 + rng() % std::min<std::size_t>(10, sz - pos);
         ex.erase(pos, len);
         oracle.erase(pos, len);
      }

      REQUIRE(ex.text() == oracle);

      // The empty document is the one intentional semantic difference: a single
      // text_layout reports 0 lines, but text_layout_ex reports 1 (an editor
      // always has at least one line for the caret). Skip that degenerate case.
      if (oracle.empty())
      {
         CHECK(ex.num_lines() == 1);
         continue;
      }

      text_layout doc{fnt, oracle};
      doc.flow(width);
      INFO("op " << op << "  ex.num_lines=" << ex.num_lines()
           << "  fresh=" << doc.num_lines());
      CHECK(ex.num_lines() == doc.num_lines());
   }
}
