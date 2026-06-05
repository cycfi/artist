/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]

   Scaling benchmark for the multi-paragraph engine (elements #370 perf): load
   alice.txt replicated Nx and time the per-operation costs that the editor hits
   while typing/scrolling, to find what scales with total document size. Tagged
   [.bench] so it does not run in the normal suite; run explicitly:
     artist_test "[bench]"
=============================================================================*/
#include "test_support.hpp"
#include <artist/text_layout.hpp>
#include <chrono>
#include <fstream>
#include <sstream>
#include <cstdio>

namespace
{
   using clk = std::chrono::steady_clock;
   double ms(clk::duration d)
   { return std::chrono::duration<double, std::milli>(d).count(); }

   std::u32string load(std::string file)
   {
      std::ifstream f(get_text_path() + file, std::ios::binary);
      std::stringstream ss; ss << f.rdbuf();
      std::string s = ss.str();
      s.erase(std::remove(s.begin(), s.end(), '\r'), s.end());
      return cycfi::to_utf32(s);
   }
}

TEST_CASE("text_layout scaling benchmark", "[.][bench]")
{
   auto const the_font = font_descr{"Open Sans", 13};
   constexpr float width = 820.0f;
   std::u32string base = load("alice.txt");
   REQUIRE(base.size() > 50000);

   std::fprintf(stderr,
      "\n%-4s %8s %8s | %9s %9s %9s %9s %9s %9s\n",
      "Nx", "chars", "paras",
      "insert", "reflow", "get_text", "numlines", "caret_rt", "draw_all");

   for (int N : {1, 2, 4})
   {
      std::u32string text;
      for (int i = 0; i < N; ++i) text += base;

      auto ex = make_text_layout(the_font, text);
      ex.flow(width);
      std::size_t P = ex.paragraph_count();
      std::size_t mid = text.size() / 2;

      // --- single-char insert at mid (the per-keystroke cost) ---
      int const K = 300;
      auto t0 = clk::now();
      for (int i = 0; i < K; ++i) ex.insert(mid, U"x");
      double t_insert = ms(clk::now() - t0) / K;
      for (int i = 0; i < K; ++i) ex.erase(mid, 1);   // restore

      // --- flow() at the SAME width (the per-keystroke waste in the widget) ---
      t0 = clk::now();
      for (int i = 0; i < 10; ++i) ex.flow(width);
      double t_reflow = ms(clk::now() - t0) / 10;

      // --- get_text() full materialization ---
      int const M = 50;
      volatile std::size_t sink = 0;
      t0 = clk::now();
      for (int i = 0; i < M; ++i) { auto s = ex.text(); sink += s.size(); }
      double t_text = ms(clk::now() - t0) / M;

      // --- num_lines() ---
      t0 = clk::now();
      for (int i = 0; i < M; ++i) sink += ex.num_lines();
      double t_nlines = ms(clk::now() - t0) / M;

      // --- caret round trip ---
      t0 = clk::now();
      for (int i = 0; i < M; ++i)
      { auto pt = ex.caret_point(mid); sink += ex.caret_index(pt); }
      double t_caret = ms(clk::now() - t0) / M;

      // --- draw the whole engine to one screen-sized offscreen (no culling) ---
      float const winh = 60 * 18.0f, winw = width + 40;
      image pm{point{winw, winh}};
      float top_y = ex.caret_point(mid).y;   // scroll to the middle
      int const F = 20;
      t0 = clk::now();
      {
         offscreen_image ctx{pm};
         canvas cnv{ctx.context()};
         for (int i = 0; i < F; ++i)
            ex.draw(cnv, {20, 20 - top_y}, colors::white);
      }
      double t_draw = ms(clk::now() - t0) / F;

      std::fprintf(stderr,
         "%-4d %8zu %8zu | %9.4f %9.4f %9.4f %9.4f %9.4f %9.4f  (ms)\n",
         N, text.size(), P, t_insert, t_reflow, t_text, t_nlines, t_caret, t_draw);
      (void)sink;
   }
   std::fprintf(stderr, "\n");
}
