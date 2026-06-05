/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]

   The source-code editing acid test for text_layout (elements #370) -- the
   original #370 use case: scripts and configuration files. Unlike prose, code
   is edited with SOFT-WRAP OFF: every '\n'-delimited line is exactly one
   display line and long lines run off to the right (horizontal scroll), they
   do not wrap. We get that by flowing with an effectively-infinite width.

   Input: code_sample.txt (a frozen C++ source file, monospace). The editing
   window follows the caret both vertically and horizontally, like a code
   editor. Same acid gate as the prose test: reconstruct a removed block of
   lines, then the text copied out of the engine must equal the original.
=============================================================================*/
#include "test_support.hpp"
#include <artist/text_layout.hpp>
#include <fstream>
#include <sstream>
#include <random>
#include <optional>
#include <string>

namespace
{
   constexpr float margin = 16.0f;
   auto const      the_font = font_descr{"Roboto Mono", 13};  // monospace
   constexpr float no_wrap = 1.0e6f;                          // disable soft-wrap
   constexpr int   win_lines = 50;
   constexpr float win_w = 900.0f;

   std::u32string load_text(std::string filename)
   {
      std::ifstream f(get_text_path() + filename, std::ios::binary);
      REQUIRE(f.good());
      std::stringstream ss;
      ss << f.rdbuf();
      std::string content = ss.str();
      content.erase(std::remove(content.begin(), content.end(), '\r'), content.end());
      return cycfi::to_utf32(content);
   }
}

TEST_CASE("text_layout source-code editing (no soft-wrap)")
{
   auto fm = font{the_font}.metrics();
   float const ascent = fm.ascent;
   float const descent = fm.descent;
   float const line_height = fm.ascent + fm.descent + fm.leading;
   float const win_h = win_lines * line_height + 2 * margin;

   std::u32string const target = load_text("code_sample.txt");
   REQUIRE(target.size() > 20000);

   std::size_t const source_lines = 1 + std::size_t(std::count(target.begin(), target.end(), U'\n'));

   std::u32string oracle = target;
   auto ex = make_text_layout(the_font, oracle);
   ex.flow(no_wrap);

   // No soft-wrap: one display line per source line.
   CHECK(ex.num_lines() == source_lines);
   CHECK(ex.paragraph_count() == source_lines);
   {
      text_run doc{the_font, oracle};
      doc.flow(no_wrap);
      CHECK(ex.num_lines() == doc.num_lines());
   }

   std::mt19937 rng(0xC0DE);

   // Editing window following the caret in BOTH axes (a code editor scrolls
   // horizontally for long lines). Optionally draws the caret.
   auto render_window =
      [&](auto const& layout, float top_x, float top_y, std::optional<point> caret) -> image
      {
         image pm{point{win_w, win_h}};
         {
            offscreen_image ctx{pm};
            canvas cnv{ctx.context()};
            cnv.add_rect({0, 0, win_w, win_h});
            cnv.fill_style(bkd_color);
            cnv.fill();
            layout.draw(cnv, {margin - top_x, margin + ascent - top_y}, colors::white);
            if (caret)
            {
               float sx = margin - top_x + caret->x;
               float sy = margin + ascent + (caret->y - top_y);
               cnv.line_width(1.5f);
               cnv.stroke_style(rgba(0, 190, 255, 255));   // elements caret
               cnv.move_to({sx, sy - ascent});
               cnv.line_to({sx, sy + descent});
               cnv.stroke();
            }
         }
         return pm;
      };

   auto images_equal = [](image const& a, image const& b) -> bool
   {
      if (!(a.bitmap_size() == b.bitmap_size())) return false;
      auto bm = a.bitmap_size();
      std::size_t n = std::size_t(bm.x) * std::size_t(bm.y);
      auto pa = a.pixels(), pb = b.pixels();
      if (!pa || !pb) return false;
      return std::equal(pa, pa + n, pb);
   };

   auto step = [&]()
   {
      REQUIRE(ex.text() == oracle);
      std::size_t paras = 1 + std::size_t(std::count(oracle.begin(), oracle.end(), U'\n'));
      CHECK(ex.paragraph_count() == paras);
      CHECK(ex.num_lines() == paras);            // no-wrap invariant: 1 line/para
   };

   auto check_layout_equiv = [&]()
   {
      text_run doc{the_font, oracle};
      doc.flow(no_wrap);
      CHECK(ex.num_lines() == doc.num_lines());

      auto fresh = make_text_layout(the_font, oracle);
      fresh.flow(no_wrap);
      CHECK(ex.num_lines() == fresh.num_lines());
      std::size_t s = std::max<std::size_t>(1, oracle.size() / 600);
      for (std::size_t i = 0; i <= oracle.size(); i += s)
      {
         auto a = fresh.caret_point(i);
         auto b = ex.caret_point(i);
         INFO("caret index " << i << " of " << oracle.size());
         CHECK(b.x == Approx(a.x).margin(0.05));
         CHECK(b.y == Approx(a.y).margin(0.05));
      }
   };

   auto snapshot = [&](std::size_t caret_idx, std::string name)
   {
      auto cp = ex.caret_point(caret_idx);
      float top_y = std::max(0.0f, cp.y - (win_lines * line_height) / 3.0f);
      float top_x = std::max(0.0f, cp.x - win_w * 0.25f);

      auto fresh = make_text_layout(the_font, oracle);
      fresh.flow(no_wrap);
      auto ex_img = render_window(ex, top_x, top_y, std::nullopt);
      auto ref_img = render_window(fresh, top_x, top_y, std::nullopt);
      ex_img.save_png(get_results_path() + name + "_ex.png");
      ref_img.save_png(get_results_path() + name + "_ref.png");
      INFO("window " << name);
      CHECK(images_equal(
         image(get_results_path() + name + "_ex.png"),
         image(get_results_path() + name + "_ref.png")));

      auto framed = render_window(ex, top_x, top_y, ex.caret_point(caret_idx));
      snapshot_golden(framed, name);
   };

   step();
   check_layout_equiv();

   // ---- build initial = target minus a block of middle lines ---------------
   std::vector<std::size_t> nl;
   for (std::size_t i = 0; i < target.size(); ++i)
      if (target[i] == U'\n') nl.push_back(i);
   REQUIRE(nl.size() > 80);
   std::size_t mid = nl.size() / 2;
   std::size_t gap_start = nl[mid] + 1;
   std::size_t gap_end = nl[mid + 30] + 1;        // remove 30 lines of code
   std::u32string missing = target.substr(gap_start, gap_end - gap_start);
   oracle = target.substr(0, gap_start) + target.substr(gap_end);
   ex.set_text(oracle);
   ex.flow(no_wrap);
   step();

   auto scratch = [&]()
   {
      std::size_t p = rng() % (oracle.size() + 1);
      std::u32string g = U"// scratch\nint x = 0;\n";
      ex.insert(p, g);       oracle.insert(p, g);          step();
      ex.erase(p, g.size());  oracle.erase(p, g.size());   step();
   };

   // ---- reconstruct the removed lines, with edit variety -------------------
   std::size_t cursor = gap_start;
   std::size_t i = 0;
   int snaps = 0;
   std::size_t next_snap = missing.size() / 5;
   while (i < missing.size())
   {
      if (rng() % 5 == 0)
         scratch();
      std::size_t n = (rng() % 3 == 0) ? 1 : (15 + rng() % 50);  // type vs paste
      n = std::min(n, missing.size() - i);
      std::u32string piece = missing.substr(i, n);
      ex.insert(cursor, piece);
      oracle.insert(cursor, piece);
      cursor += n;
      i += n;
      step();
      if (i >= next_snap)
      {
         snapshot(cursor, "code_" + std::to_string(snaps));
         check_layout_equiv();
         ++snaps;
         next_snap += missing.size() / 5;
      }
   }

   // ---- FINAL ACID GATE ----------------------------------------------------
   REQUIRE(oracle == target);
   REQUIRE(ex.text() == target);
   CHECK(ex.num_lines() == source_lines);         // back to the original line count
   check_layout_equiv();
   snapshot(cursor, "code_final");
}
