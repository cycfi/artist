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

      for (int k = 0; k < 2000; ++k)
      {
         std::size_t i = rng() % r.size();
         CHECK(r[i] == oracle[i]);
      }
   }

   if (failures == 0) std::cout << "ALL PASS\n";
   else               std::cout << failures << " FAILURE(S)\n";
   return failures ? 1 : 0;
}
