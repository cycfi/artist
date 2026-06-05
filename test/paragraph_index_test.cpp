/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]

   Unit test for cycfi::artist::detail::paragraph_index: the incremental
   paragraph partition that lets text_layout relayout only edited
   paragraphs (elements #370). Non-graphical.
=============================================================================*/
#include <artist/detail/paragraph_index.hpp>
#include <artist/rope.hpp>
#include <string>
#include <vector>
#include <random>
#include <iostream>

using cycfi::artist::rope;
using cycfi::artist::detail::paragraph_index;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { \
   std::cerr << "FAIL " << __LINE__ << ": " #cond "\n"; ++failures; } } while (0)

static rope<char32_t> make_rope(std::u32string const& s)
{
   return rope<char32_t>(s.begin(), s.end());
}

// Oracle: paragraph start offsets of a flat string. Paragraphs are separated
// by '\n'; a trailing '\n' yields a final empty paragraph.
static std::vector<std::size_t> oracle_starts(std::u32string const& s)
{
   std::vector<std::size_t> starts{0};
   for (std::size_t i = 0; i < s.size(); ++i)
      if (s[i] == U'\n')
         starts.push_back(i + 1);
   return starts;
}

int main()
{
   // --- build + basic queries ---
   {
      std::u32string s = U"alpha\nbeta\ngamma";
      auto buf = make_rope(s);
      paragraph_index px;
      px.build(buf);
      CHECK(px.count() == 3);
      CHECK(px.start(0) == 0  && px.end(0) == 6);   // "alpha\n"
      CHECK(px.start(1) == 6  && px.end(1) == 11);  // "beta\n"
      CHECK(px.start(2) == 11 && px.end(2) == 16);  // "gamma"
      CHECK(px.index_at(0) == 0);
      CHECK(px.index_at(5) == 0);
      CHECK(px.index_at(6) == 1);
      CHECK(px.index_at(16) == 2);                  // end maps to last paragraph
      CHECK(px.starts() == oracle_starts(s));
   }

   // --- trailing newline => empty final paragraph ---
   {
      auto buf = make_rope(U"x\n");
      paragraph_index px; px.build(buf);
      CHECK(px.count() == 2);
      CHECK(px.start(1) == 2 && px.end(1) == 2);    // empty
   }
   {
      auto buf = make_rope(U"");
      paragraph_index px; px.build(buf);
      CHECK(px.count() == 1);
      CHECK(px.start(0) == 0 && px.end(0) == 0);
   }

   // --- incremental update reports the changed paragraph range ---
   {
      std::u32string s = U"one\ntwo\nthree";
      auto buf = make_rope(s);
      paragraph_index px; px.build(buf);

      // Type 'X' inside paragraph 1 ("two" -> "tXwo"): only paragraph 1 changes.
      buf.insert(5, {U'X'});                 // pos 5 is inside "two"
      auto ch = px.update(buf, 5, 0, 1);
      CHECK(ch.first == 1 && ch.last == 1);
      CHECK(px.starts() == oracle_starts(U"one\ntXwo\nthree"));

      // Insert a newline in paragraph 0 ("one" -> "o\nne"): splits 0 into two.
      buf.insert(1, {U'\n'});
      auto ch2 = px.update(buf, 1, 0, 1);
      CHECK(ch2.first == 0 && ch2.last == 1); // one paragraph became two
      CHECK(px.starts() == oracle_starts(U"o\nne\ntXwo\nthree"));
   }

   // --- erasing a newline merges paragraphs ---
   {
      auto buf = make_rope(U"aa\nbb\ncc");
      paragraph_index px; px.build(buf);
      CHECK(px.count() == 3);
      buf.erase(2, 1);                       // remove the first '\n' -> "aabb\ncc"
      auto ch = px.update(buf, 2, 1, 0);
      CHECK(px.count() == 2);
      CHECK(px.starts() == oracle_starts(U"aabb\ncc"));
      CHECK(ch.first == 0);                  // paragraph 0 (merged) changed
   }

   // --- differential fuzz: incremental update must match build-from-scratch ---
   {
      std::mt19937 rng(20260605);
      std::u32string oracle = U"start\nline";
      auto buf = make_rope(oracle);
      paragraph_index px; px.build(buf);

      auto rnd = [&](std::size_t n) {
         std::u32string s;
         for (std::size_t i = 0; i < n; ++i)
         {
            unsigned r = rng() % 12;
            s.push_back(r == 0 ? U'\n' : char32_t(U'a' + r)); // ~1/12 newlines
         }
         return s;
      };

      for (int op = 0; op < 20000; ++op)
      {
         std::size_t sz = oracle.size();
         if ((rng() & 1) || sz == 0)
         {
            std::size_t pos = sz ? rng() % (sz + 1) : 0;
            auto t = rnd(1 + rng() % 6);
            buf.insert(pos, t.begin(), t.end());
            oracle.insert(pos, t);
            px.update(buf, pos, 0, t.size());
         }
         else
         {
            std::size_t pos = rng() % sz;
            std::size_t len = 1 + rng() % (sz - pos);
            buf.erase(pos, len);
            oracle.erase(pos, len);
            px.update(buf, pos, len, 0);
         }
         CHECK(px.starts() == oracle_starts(oracle));
         if (px.starts() != oracle_starts(oracle)) break;   // stop spam
      }
      std::cout << "fuzz paragraphs=" << px.count()
                << " buf=" << buf.size() << "\n";
   }

   if (failures == 0) std::cout << "ALL PASS\n";
   else               std::cout << failures << " FAILURE(S)\n";
   return failures ? 1 : 0;
}
