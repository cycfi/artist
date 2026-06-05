/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]

   The acid test for text_layout_ex (elements #370): a full editing session
   against the real graphics backend over a novel-length document.

   Build strategy: load the whole text MINUS a block of paragraphs removed from
   the middle, then simulate an editing session -- typing, pasting, plus
   net-zero "scratch" edits elsewhere -- that reinserts the missing block. A
   std::u32string oracle is maintained in lockstep, so the document converges
   back to the original; the final, decisive check is a full-text comparison:
   text copied out of the engine must equal the original.

   Verification:
     - every step: engine text == oracle; paragraph_count == newline count + 1.
     - whole-document correctness (cheap, total): num_lines and a caret-geometry
       sweep over the entire document must match a freshly built text_layout of
       the same text -- catches incremental-update drift even when text matches.
     - render correctness + goldens are EDIT-LOCAL: an 80-line caret-following
       window (one editor screen, caret near the top third) is rendered; its
       text must be PIXEL-IDENTICAL to the same window of the ground-truth
       text_layout, and the window WITH the caret drawn is golden-snapshotted
       (saved first run, compared after).

   Inputs (test/text/, see SOURCE.md): alice.txt (prose, soft-wrap).
=============================================================================*/
#include "test_support.hpp"
#include <artist/text_layout_ex.hpp>
#include <fstream>
#include <sstream>
#include <random>
#include <optional>
#include <string>

namespace
{
   constexpr float margin = 20.0f;
   auto const      the_font = font_descr{"Open Sans", 13};
   constexpr float flow_width = 560.0f;
   constexpr int   win_lines = 80;            // one (generous) editor screen

   std::u32string load_text(std::string filename)
   {
      std::ifstream f(get_text_path() + filename, std::ios::binary);
      REQUIRE(f.good());
      std::stringstream ss;
      ss << f.rdbuf();
      return cycfi::to_utf32(ss.str());
   }
}

TEST_CASE("text_layout_ex novel editing session reconstructs the original")
{
   auto fm = font{the_font}.metrics();
   float const ascent = fm.ascent;
   float const descent = fm.descent;
   float const line_height = fm.ascent + fm.descent + fm.leading;
   float const win_w = flow_width + 2 * margin;
   float const win_h = win_lines * line_height + 2 * margin;

   std::u32string const target = load_text("alice.txt");
   REQUIRE(target.size() > 50000);            // a real, long document

   // ---- build `initial` = target minus a block of middle paragraphs --------
   std::vector<std::size_t> nl;
   for (std::size_t i = 0; i < target.size(); ++i)
      if (target[i] == U'\n') nl.push_back(i);
   REQUIRE(nl.size() > 60);
   std::size_t mid = nl.size() / 2;
   std::size_t gap_start = nl[mid] + 1;       // start of a middle paragraph
   std::size_t gap_end = nl[mid + 20] + 1;    // ...20 paragraphs later
   std::u32string missing = target.substr(gap_start, gap_end - gap_start);
   std::u32string initial = target.substr(0, gap_start) + target.substr(gap_end);

   std::u32string oracle = initial;
   auto ex = make_text_layout_ex(the_font, oracle);
   ex.flow(flow_width);

   std::mt19937 rng(0x0A11CE);

   // Render an 80-line window of `layout`, scrolled so document-y `top_y` is at
   // the top; optionally draw a caret at document point `caret`.
   auto render_window =
      [&](auto const& layout, float top_y, std::optional<point> caret) -> image
      {
         image pm{point{win_w, win_h}};
         {
            offscreen_image ctx{pm};
            canvas cnv{ctx.context()};
            cnv.add_rect({0, 0, win_w, win_h});
            cnv.fill_style(bkd_color);
            cnv.fill();
            layout.draw(cnv, {margin, margin + ascent - top_y}, colors::black);
            if (caret)
            {
               float sx = margin + caret->x;
               float sy = margin + ascent + (caret->y - top_y);  // baseline
               cnv.line_width(1.5f);
               cnv.stroke_style(rgba(230, 70, 70, 255));
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

   // ---- per-step invariants (cheap, every edit) ----------------------------
   auto step = [&]()
   {
      REQUIRE(ex.text() == oracle);
      std::size_t paras = 1 + std::size_t(std::count(oracle.begin(), oracle.end(), U'\n'));
      CHECK(ex.paragraph_count() == paras);
      CHECK(ex.num_lines() >= paras);
   };

   // ---- whole-document layout equivalence vs a fresh single text_layout ----
   auto check_layout_equiv = [&]()
   {
      text_layout doc{the_font, oracle};
      doc.flow(flow_width);
      CHECK(ex.num_lines() == doc.num_lines());
      std::size_t s = std::max<std::size_t>(1, oracle.size() / 600);
      for (std::size_t i = 0; i <= oracle.size(); i += s)
      {
         auto a = doc.caret_point(i);
         auto b = ex.caret_point(i);
         INFO("caret index " << i << " of " << oracle.size());
         CHECK(b.x == Approx(a.x).margin(0.1));
         CHECK(b.y == Approx(a.y).margin(0.1));
      }
   };

   // ---- edit-local snapshot: 80-line window following the caret ------------
   auto snapshot = [&](std::size_t caret_idx, std::string name)
   {
      float caret_y = ex.caret_point(caret_idx).y;
      float third = (win_lines * line_height) / 3.0f;
      float top_y = std::max(0.0f, caret_y - third);

      // text must render identically to the ground-truth single text_layout
      text_layout doc{the_font, oracle};
      doc.flow(flow_width);
      auto ex_text = render_window(ex, top_y, std::nullopt);
      auto doc_text = render_window(doc, top_y, std::nullopt);
      ex_text.save_png(get_results_path() + name + "_ex.png");
      doc_text.save_png(get_results_path() + name + "_doc.png");
      INFO("window snapshot " << name);
      CHECK(images_equal(
         image(get_results_path() + name + "_ex.png"),
         image(get_results_path() + name + "_doc.png")));

      // golden the editor-like window, caret drawn
      auto framed = render_window(ex, top_y, ex.caret_point(caret_idx));
      snapshot_golden(framed, name);
   };

   step();
   check_layout_equiv();

   // ---- reconstruct the missing block at the gap, with edit variety --------
   auto scratch = [&]()
   {
      std::size_t p = rng() % (oracle.size() + 1);
      std::u32string g;
      for (int j = 0, n = 1 + int(rng() % 6); j < n; ++j)
         g.push_back(char32_t(U'a' + rng() % 26));
      ex.insert(p, g);       oracle.insert(p, g);          step();
      ex.erase(p, g.size());  oracle.erase(p, g.size());   step();
      if (rng() % 3 == 0)
      {
         std::u32string chunk = U"scratch one\n\nscratch two\n";
         std::size_t q = rng() % (oracle.size() + 1);
         ex.insert(q, chunk);       oracle.insert(q, chunk);         step();
         ex.erase(q, chunk.size());  oracle.erase(q, chunk.size());  step();
      }
   };

   std::size_t cursor = gap_start;
   std::size_t i = 0;
   int snaps = 0;
   std::size_t next_snap = missing.size() / 6;
   while (i < missing.size())
   {
      if (rng() % 4 == 0)
         scratch();

      std::size_t n = (rng() % 3 == 0) ? 1 : (20 + rng() % 80);  // type vs paste
      n = std::min(n, missing.size() - i);
      std::u32string piece = missing.substr(i, n);
      ex.insert(cursor, piece);
      oracle.insert(cursor, piece);
      cursor += n;
      i += n;
      step();

      if (i >= next_snap)                       // caret-following snapshot
      {
         snapshot(cursor, "novel_alice_" + std::to_string(snaps));
         check_layout_equiv();
         ++snaps;
         next_snap += missing.size() / 6;
      }
   }

   // ====================================================================
   // FINAL ACID GATE
   // ====================================================================

   // 1) Decisive: text copied out of the engine equals the original.
   REQUIRE(oracle == target);
   REQUIRE(ex.text() == target);

   // 2) Whole-document layout equivalence vs a fresh build of the original.
   check_layout_equiv();

   // 3) Final edit-local snapshot at the reconstructed region.
   snapshot(cursor, "novel_alice_final");
}
