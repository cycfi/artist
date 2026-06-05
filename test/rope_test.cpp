/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]

   Unit test for cycfi::artist::rope (text engine buffer, elements #370).
   Non-graphical: builds and runs without a window or graphics backend.
=============================================================================*/
#include <artist/rope.hpp>
#include <string>
#include <vector>
#include <random>
#include <iostream>

using cycfi::artist::rope;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { \
   std::cerr << "FAIL " << __LINE__ << ": " #cond "\n"; ++failures; } } while (0)

// Build a vector<char32_t> from an ASCII literal (one code point per char).
static std::vector<char32_t> V(char const* s)
{
   std::vector<char32_t> v;
   for (; *s; ++s) v.push_back(char32_t(*s));
   return v;
}

static std::vector<char32_t> flat(rope<char32_t> const& r)
{
   return r.flatten();
}

int main()
{
   // --- construction & basic queries ---
   {
      rope<char32_t> r;                         // default: empty
      CHECK(r.size() == 0);
      CHECK(r.empty());
      CHECK(flat(r).empty());
   }
   {
      auto s = V("hello");
      rope<char32_t> r(s.begin(), s.end());
      CHECK(r.size() == 5);
      CHECK(!r.empty());
      CHECK(flat(r) == s);
      CHECK(r[0] == U'h');
      CHECK(r[4] == U'o');
   }

   // --- insert at front, middle, end ---
   {
      rope<char32_t> r(V("world"));
      auto h = V("hello ");
      r.insert(0, h.begin(), h.end());          // "hello world"
      CHECK(flat(r) == V("hello world"));
      auto bang = V("!");
      r.insert(r.size(), bang.begin(), bang.end());   // "hello world!"
      CHECK(flat(r) == V("hello world!"));
      auto comma = V(",");
      r.insert(5, comma.begin(), comma.end());        // "hello, world!"
      CHECK(flat(r) == V("hello, world!"));
   }

   // --- erase across leaf boundaries ---
   {
      rope<char32_t> r(V("abcdef"));
      auto xyz = V("XYZ");
      r.insert(3, xyz.begin(), xyz.end());      // "abcXYZdef"
      CHECK(flat(r) == V("abcXYZdef"));
      r.erase(2, 5);                            // remove "cXYZd" -> "abef"
      CHECK(flat(r) == V("abef"));
      r.erase(0, 100);                          // clamp
      CHECK(r.empty());
   }

   // --- subrope (slice) ---
   {
      rope<char32_t> r(V("abc"));
      auto x = V("123");
      r.insert(1, x.begin(), x.end());          // "a123bc"
      CHECK(flat(r.subrope(0, 6)) == V("a123bc"));
      CHECK(flat(r.subrope(1, 3)) == V("123"));
      CHECK(flat(r.subrope(4, 10)) == V("bc"));
   }

   // --- persistence: a copy is an O(1) snapshot, independent of later edits ---
   {
      rope<char32_t> r(V("snapshot"));
      rope<char32_t> snap = r;                  // share structure
      auto z = V("ZZ");
      r.insert(4, z.begin(), z.end());          // mutate r only
      CHECK(flat(r) == V("snapZZshot"));
      CHECK(flat(snap) == V("snapshot"));       // snapshot unchanged
   }

   // --- differential fuzz against a std::u32string oracle ---
   {
      std::mt19937 rng(987654321);
      rope<char32_t> r(V("seed"));
      std::u32string oracle = U"seed";
      auto rnd = [&](std::size_t n) {
         std::vector<char32_t> s;
         for (std::size_t i = 0; i < n; ++i) s.push_back(U"abcdefg"[rng() % 7]);
         return s;
      };
      for (int op = 0; op < 20000; ++op)
      {
         std::size_t sz = oracle.size();
         if ((rng() & 1) || sz == 0)
         {
            std::size_t pos = sz ? rng() % (sz + 1) : 0;
            auto t = rnd(1 + rng() % 5);
            r.insert(pos, t.begin(), t.end());
            oracle.insert(pos, t.data(), t.size());
         }
         else
         {
            std::size_t pos = rng() % sz;
            std::size_t len = 1 + rng() % (sz - pos);
            r.erase(pos, len);
            oracle.erase(pos, len);
         }
         CHECK(r.size() == oracle.size());
      }
      auto fv = flat(r);
      std::u32string got(fv.begin(), fv.end());
      CHECK(got == oracle);
      CHECK(r.max_leaf_size() <= 256);
      std::cout << "fuzz final size=" << r.size() << " depth=" << r.depth() << "\n";
   }

   // --- large-buffer stress: force a real tree and assert it stays balanced ---
   {
      std::mt19937 rng(424242);
      auto rnd = [&](std::size_t n) {
         std::vector<char32_t> s;
         for (std::size_t i = 0; i < n; ++i) s.push_back(U"abcdefghij"[rng() % 10]);
         return s;
      };
      rope<char32_t> r;
      std::u32string oracle;
      while (oracle.size() < 60000)
      {
         std::size_t pos = oracle.empty() ? 0 : rng() % (oracle.size() + 1);
         auto t = rnd(20 + rng() % 60);
         r.insert(pos, t.begin(), t.end());
         oracle.insert(pos, t.data(), t.size());
      }
      for (int op = 0; op < 20000; ++op)
      {
         std::size_t sz = oracle.size();
         if ((rng() & 1) || sz < 40000)
         {
            std::size_t pos = sz ? rng() % (sz + 1) : 0;
            auto t = rnd(1 + rng() % 30);
            r.insert(pos, t.begin(), t.end());
            oracle.insert(pos, t.data(), t.size());
         }
         else
         {
            std::size_t pos = rng() % sz;
            std::size_t len = 1 + rng() % std::min<std::size_t>(50, sz - pos);
            r.erase(pos, len);
            oracle.erase(pos, len);
         }
      }
      auto fv = flat(r);
      std::u32string got(fv.begin(), fv.end());
      CHECK(got == oracle);

      std::size_t ideal = 0;
      for (std::size_t s = r.size(); s > 1; s >>= 1) ++ideal;
      std::cout << "stress size=" << r.size() << " depth=" << r.depth()
                << " ideal~" << ideal << "\n";
      CHECK(r.depth() <= 3 * ideal + 8);
      CHECK(r.max_leaf_size() <= 256);

      for (int k = 0; k < 2000; ++k)
      {
         std::size_t i = rng() % r.size();
         CHECK(r[i] == oracle[i]);
      }
   }

   // ======================================================================
   // Text-editing use-case API + invariants
   // ======================================================================

   // Leaf invariant: no leaf ever exceeds rope_max_leaf (256). This must hold
   // after construction, insert (incl. large paste), and erase.
   {
      // Load: constructing from large input must chunk, not make one giant leaf.
      std::vector<char32_t> big(10000, U'x');
      rope<char32_t> r(big.begin(), big.end());
      CHECK(r.size() == 10000);
      CHECK(r.max_leaf_size() <= 256);          // chunked on load
      CHECK(r.leaf_count() >= 10000 / 256);     // really multiple leaves
      CHECK(r.depth() > 1);

      // Large paste must also chunk.
      std::vector<char32_t> paste(5000, U'y');
      r.insert(5000, paste.begin(), paste.end());
      CHECK(r.size() == 15000);
      CHECK(r.max_leaf_size() <= 256);
   }

   // Coalescing: sequential single-char typing into a LARGE document must not
   // create one leaf per character.  Leaf count should stay ~ size/leaf, and
   // every leaf must remain within the cap.
   {
      std::vector<char32_t> base(50000, U'.');
      rope<char32_t> r(base.begin(), base.end());
      std::u32string oracle(50000, U'.');
      std::size_t caret = 25000;
      for (int i = 0; i < 3000; ++i)            // type 3000 chars near the middle
      {
         char32_t ch = U"abcde"[i % 5];
         r.insert(caret, &ch, &ch + 1);
         oracle.insert(oracle.begin() + caret, ch);
         ++caret;
      }
      auto fv = flat(r);
      CHECK(std::u32string(fv.begin(), fv.end()) == oracle);
      CHECK(r.max_leaf_size() <= 256);
      // If each keystroke made its own leaf we'd have ~53000 leaves; coalescing
      // must keep it near size/leaf.
      std::cout << "typing leaf_count=" << r.leaf_count()
                << " (size=" << r.size() << ")\n";
      CHECK(r.leaf_count() <= r.size() / 64);
   }

   // substr: materialized slice (clipboard copy/cut, run text).
   {
      rope<char32_t> r(V("abc"));
      auto x = V("123");
      r.insert(1, x.begin(), x.end());          // "a123bc"
      CHECK(r.substr(0, 6) == V("a123bc"));
      CHECK(r.substr(1, 3) == V("123"));
      CHECK(r.substr(4, 10) == V("bc"));        // clamp
      CHECK(r.substr(100, 5).empty());          // out of range
   }

   // for_each: read a range without materializing; must equal substr.
   {
      std::vector<char32_t> base(2000);
      for (std::size_t i = 0; i < base.size(); ++i) base[i] = U'a' + (i % 26);
      rope<char32_t> r(base.begin(), base.end());
      std::vector<char32_t> got;
      r.for_each(100, 1500, [&](char32_t const* p, std::size_t n)
         { got.insert(got.end(), p, p + n); });
      CHECK(got == r.substr(100, 1500));
      CHECK(got.size() == 1500);
   }

   if (failures == 0) std::cout << "ALL PASS\n";
   else               std::cout << failures << " FAILURE(S)\n";
   return failures ? 1 : 0;
}
