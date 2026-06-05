#include "test_support.hpp"

void typography(canvas& cnv)
{
   background(cnv);

   cnv.fill_style(rgba(220, 220, 220, 200));
   cnv.stroke_style(rgba(220, 220, 220, 200));

   // Regular
   cnv.font(font_descr{"Open Sans", 36});
   cnv.fill_text("Regular", 20, 40);

   // Bold
   cnv.font(font_descr{"Open Sans", 36}.bold());
   cnv.fill_text("Bold", 160, 40);

   // Light
   cnv.font(font_descr{"Open Sans", 36}.light());
   cnv.fill_text("Light", 250, 40);

   // Italic
   cnv.font(font_descr{"Open Sans", 36}.italic());
   cnv.fill_text("Italic", 345, 40);

   // Condensed
   cnv.font(font_descr{"Open Sans Condensed, Open Sans", 36}.condensed());
   cnv.fill_text("Condensed", 430, 40);

   // Condensed Italic
   cnv.font(font_descr{"Open Sans Condensed, Open Sans", 36}.italic().condensed());
   cnv.fill_text("Condensed Italic", 20, 115);

   // In the last two cases, the font family 'Open Sans Condensed' already
   // describes the font as condensed, but we still add the font family 'Open
   // Sans' and styles because, depending on platform, the font family 'Open
   // Sans Condensed' is either grouped into the 'Open Sans' family (e.g.
   // Windows), or separate as a distinct family (e.g. MacOS).

   // Outline
   cnv.font(font_descr{"Open Sans", 36}.bold());
   cnv.line_width(0.5);
   cnv.stroke_text("Outline", 210, 115);

   cnv.font(font_descr{"Open Sans", 52}.bold());

   // Gradient Fill
   {
      auto gr = canvas::linear_gradient{{360, 90}, {360, 140}};
      gr.add_color_stop(0.0, colors::navy_blue);
      gr.add_color_stop(1.0, colors::maroon);
      cnv.fill_style(gr);
      cnv.fill_text("Gradient", 360, 115);
      cnv.stroke_text("Gradient", 360, 115);
   }

   // Outline Gradient Fill
   {
      auto gr = canvas::linear_gradient{{360, 165}, {360, 215}};
      gr.add_color_stop(0.0, colors::medium_blue);
      gr.add_color_stop(1.0, colors::medium_violet_red);
      cnv.line_width(1.5);
      cnv.stroke_style(gr);
      cnv.stroke_text("Outline Gradient", 20, 190);
   }

#if defined(ARTIST_QUARTZ_2D) // CoreText supports ligatures right out of the box, but only for some fonts
   cnv.font(font_descr{"Lucida Grande", 52}.bold());
#else
   cnv.font(font_descr{"Open Sans", 52}.bold());
#endif

   cnv.fill_style(rgba(220, 220, 220, 200));
   cnv.fill_text("fi fl", 500, 190);

   cnv.font(font_descr{"Open Sans", 52}.weight(semi_bold));
   {
      auto state = cnv.new_state();

      // Shadow
      cnv.fill_style(rgba(220, 220, 220, 200));
      cnv.shadow_style({5.0, 5.0}, 5, colors::black);
      cnv.fill_text("Shadow", 20, 265);

      // Glow
      cnv.fill_style(bkd_color);
      cnv.shadow_style(8, colors::light_sky_blue);
      cnv.fill_text("Glow", 250, 265);

      auto m = cnv.measure_text("Shadow");
      CHECK(std::floor(m.ascent) == 55);
      CHECK(std::floor(m.descent) == 15);
      CHECK(std::floor(m.leading) == 0);
      CHECK(std::floor(m.size.x) == 198);
      CHECK(std::floor(m.size.y) == 70);
   }

   cnv.move_to({500, 220});
   cnv.line_to({500, 480});
   cnv.stroke_style(colors::red);
   cnv.line_width(0.5);
   cnv.stroke();

   cnv.fill_style(rgba(220, 220, 220, 200));
   cnv.font(font_descr{"Open Sans", 14});

   char const* align_text[] = {
      "text_align(left)",
      "text_align(right)",
      "text_align(center)",
      "text_align(baseline)",
      "text_align(top)",
      "text_align(middle)",
      "text_align(bottom)"
   };

   int aligns[] = {
      cnv.left,
      cnv.right,
      cnv.center,
      cnv.baseline,
      cnv.top,
      cnv.middle,
      cnv.bottom
   };

   float vspace = 35;
   float vstart = 250-vspace;
   for (int i = 0; i != 7; ++i)
   {
      vstart += vspace;
      cnv.move_to({400, vstart});
      cnv.line_to({600, vstart});
      cnv.stroke();
      cnv.text_align(aligns[i]);
      cnv.fill_text(align_text[i], 500, vstart);
   }

   std::string text =
      "Although I am a typical loner in daily life, my consciousness of "
      "belonging to the invisible community of those who strive for "
      "truth, beauty, and justice has preserved me from feeling isolated.\n\n"

      "The years of anxious searching in the dark, with their intense "
      "longing, their alternations of confidence and exhaustion, and "
      "final emergence into light—only those who have experienced it "
      "can understand that.\n\n"

      "⁠—Albert Einstein"
      ;

   auto tlayout = text_layout{
      font_descr{"Open Sans", 12}.italic()
    , text
   };
   tlayout.flow(350, true);
   tlayout.draw(cnv, 20, 300, rgba(220, 220, 220, 200));

   // Hit testing
   {
      auto i = tlayout.caret_index(0, 2000);
      CHECK(i == tlayout.npos);

      i = tlayout.caret_index(0, 0);
      CHECK(i == 0);

      i = tlayout.caret_index(350, 0);
      CHECK(i == 64);
      CHECK(text[i] == ' ');

      i = tlayout.caret_index(0, 20);
      CHECK(i == 133);
      CHECK(text[i] == 'b');

      i = tlayout.caret_index(5, 20);
      CHECK(i == 134);
      CHECK(text[i] == 'e');

      i = tlayout.caret_index(350, 20);
      CHECK(i == 192);
      CHECK(text[i] == '\n');

      i = tlayout.caret_index(109, 20);
      CHECK(i == 154);
      CHECK(text[i] == 'a');

      i = tlayout.caret_index(350, 15);
      CHECK(i == 132);
      CHECK(text[i] == ' ');

      // Harfbuzz vs. CoreText have slightly different results here,
      // but that is OK.
      i = tlayout.caret_index(343, 15);
      auto check_index = i == 131 || i == 130;
      auto check_char = text[i] == ',' || text[i] == 'h';
      CHECK(check_index);
      CHECK(check_char);

      i = tlayout.caret_index(20, 49);
      CHECK(i == 193);
      CHECK(text[i] == '\n');

      i = tlayout.caret_index(0, 147);
      CHECK(i == 403);
      char32_t cp = *(tlayout.text().data()+i);
      CHECK(cp == 8288); // 'WORD JOINER' (U+2060)

      i = tlayout.caret_index(88, 147);
      CHECK(i == tlayout.text().size());
   }

   // glyph_bounds
   {
      auto test_caret_pos =
         [&tlayout](std::size_t index, bool exceed = false)
         {
            point pos = tlayout.caret_point(index);
            if (exceed)
            {
               CHECK(std::floor(pos.x) == 86);
               CHECK(std::floor(pos.y) == 147);
            }
            else
            {
               std::size_t got_index = tlayout.caret_index(pos);
               CHECK(got_index == index);
            }
         };

      test_caret_pos(1000, true);
      test_caret_pos(-1, true);

      test_caret_pos(10);
      test_caret_pos(0);
      test_caret_pos(64);
      test_caret_pos(193);
      test_caret_pos(132);
      test_caret_pos(133);
      test_caret_pos(154);
      test_caret_pos(325);
      test_caret_pos(326);
      test_caret_pos(405);
      test_caret_pos(420);
   }
}

TEST_CASE("Typography")
{
   image pm{window_size};
   {
      offscreen_image ctx{pm};
      canvas pm_cnv{ctx.context()};

      auto p = pm_cnv.device_to_user(100, 100);

      typography(pm_cnv);
   }
   compare_golden(pm, "typography");
}
TEST_CASE("Text Shaping")
{
   image pm{window_size};
   offscreen_image ctx{pm};
   canvas cnv{ctx.context()};

   // Basic shaping sanity: positive advance, longer text wider than shorter.
   {
      cnv.font(font_descr{"Open Sans", 36});
      auto m1 = cnv.measure_text("A");
      auto m2 = cnv.measure_text("AA");
      auto m3 = cnv.measure_text("AAA");
      CHECK(m1.size.x > 0);
      CHECK(m2.size.x > m1.size.x);
      CHECK(m3.size.x > m2.size.x);
   }

   // Ligature / kern: "fi" must not be wider than "f"+"i" measured separately.
   // With HarfBuzz liga enabled, "fi" is typically narrower (ligature substitution).
   // Without liga it is still ≤ sum because no negative-kern penalty applies.
   {
      cnv.font(font_descr{"Open Sans", 36}.bold());
      auto mf  = cnv.measure_text("f");
      auto mi  = cnv.measure_text("i");
      auto mfi = cnv.measure_text("fi");
      CHECK(mf.size.x > 0);
      CHECK(mi.size.x > 0);
      CHECK(mfi.size.x <= mf.size.x + mi.size.x + 1.0f);  // 1px tolerance
   }

   // text_layout::num_lines() must reflect reflow width.
   // "Hello World" at Open Sans 14 fits on one line at 640px, two or more at 1px.
   {
      {
         text_layout tl{font_descr{"Open Sans", 14}, "Hello World"};
         tl.flow(640, false);
         CHECK(tl.num_lines() == 1);
      }
      {
         text_layout tl{font_descr{"Open Sans", 14}, "Hello World"};
         tl.flow(1, false);
         CHECK(tl.num_lines() >= 2);
      }
   }

   // Narrower flow always produces at least as many lines as wider flow.
   {
      std::string const txt =
         "The quick brown fox jumps over the lazy dog";
      text_layout wide {font_descr{"Open Sans", 14}, txt};
      text_layout narrow{font_descr{"Open Sans", 14}, txt};
      wide.flow(640, false);
      narrow.flow(100, false);
      CHECK(narrow.num_lines() >= wide.num_lines());
   }

   // caret_point(0) is the start of the first glyph (x near 0, y >= 0).
   {
      text_layout tl{font_descr{"Open Sans", 14}, "Hello"};
      tl.flow(640, false);
      auto p = tl.caret_point(0);
      CHECK(p.x >= 0.0f);
      CHECK(p.y >= 0.0f);
   }

   // caret_point advances monotonically left-to-right for simple ASCII.
   {
      std::string const txt = "Hello";
      text_layout tl{font_descr{"Open Sans", 14}, txt};
      tl.flow(640, false);
      float prev_x = -1.0f;
      for (std::size_t i = 0; i < txt.size(); ++i)
      {
         auto p = tl.caret_point(i);
         CHECK(p.x >= prev_x);
         prev_x = p.x;
      }
   }
}

TEST_CASE("Word Selection")
{
   // word_break(i) marks a UAX-29 word boundary AFTER character i.  This
   // reference selection mirrors the elements double-click handler: it returns
   // the [start, end) word containing a click.  It verifies the boundary data
   // drives correct whole-word selection across contractions, decimals,
   // grouping separators and punctuation.
   auto select = [](std::string const& txt, std::size_t click) -> std::string
   {
      text_layout tl{font_descr{"Open Sans", 14}, txt};
      std::size_t n = txt.size();
      auto is_wb = [&](std::size_t i)
         { return tl.word_break(i) == text_layout::allow_break; };
      std::size_t end = click;
      while (end < n && !is_wb(end)) ++end;
      if (end < n) ++end;                         // boundary is after char `end`
      std::size_t start = click;
      while (start > 0 && !is_wb(start - 1)) --start;
      return txt.substr(start, end - start);
   };

   CHECK(select("To traverse the", 5)  == "traverse");  // mid-word
   CHECK(select("To traverse the", 10) == "traverse");  // click last letter
   CHECK(select("To traverse the", 0)  == "To");        // first word
   CHECK(select("don't", 2)            == "don't");      // contraction kept
   CHECK(select("3.14", 1)             == "3.14");       // decimal kept
   CHECK(select("1,000", 3)            == "1,000");      // grouping kept
   CHECK(select("foo,bar", 1)          == "foo");        // punctuation boundary
   CHECK(select("foo,bar", 5)          == "bar");
   CHECK(select("e-mail", 3)           == "mail");       // hyphen boundary
   CHECK(select("resume", 2)           == "resume");
}

TEST_CASE("CJK line wrapping")
{
   // Issue cycfi/elements#430: CJK text without spaces that overflows the flow
   // width must wrap WITHOUT dropping characters.  Each ideograph is its own
   // line-break opportunity, so the break must keep the boundary glyph on the
   // line (unlike a space, which is consumed).  This also exercises the
   // force-break path, which previously read past the end of the glyph array.

   // 8 ideographs (one codepoint each), repeated 8 times => 64 codepoints.
   std::string cjk;
   for (int i = 0; i != 8; ++i)
      cjk += "开放包容视野宽广";
   std::size_t const n = 64;

   text_layout tl{font_descr{"Open Sans", 14}, cjk};
   tl.flow(40, false);                 // narrow: force many wraps

   // It must actually wrap into multiple lines.
   CHECK(tl.num_lines() >= 2);

   // Distinct row y-positions, in visual (top-to-bottom) order.
   std::vector<float> rows;
   for (std::size_t i = 0; i <= n; ++i)
   {
      float y = tl.caret_point(i).y;
      if (rows.empty() || y > rows.back())
         rows.push_back(y);
   }
   CHECK(rows.size() == tl.num_lines());

   // No character may be dropped at a wrap: the lines must cover a contiguous
   // run of characters (there is no whitespace between ideographs to consume).
   // The index one past the right edge of each line must equal the index at the
   // left edge of the next line, on every backend.
   for (std::size_t r = 0; r + 1 < rows.size(); ++r)
   {
      auto right = tl.caret_index(1e6f, rows[r]);
      auto left  = tl.caret_index(-1e6f, rows[r+1]);
      CHECK(right == left);
   }
}

TEST_CASE("Ligature at end of line")
{
   // Issue cycfi/elements#384: a string that ENDS in a ligature (e.g. the "fi"
   // of "wifi" shapes to a single glyph spanning two code points) made the whole
   // text disappear on the Skia backend.
   //
   // End-of-text is detected from libunibreak's INDETERMINATE marker, which sits
   // on the final code point.  When that code point is swallowed by a trailing
   // ligature it carries no glyph, so the marker was never seen and the final
   // line was never flushed -> num_lines() == 0 and nothing was drawn.

   text_layout tl{font_descr{"Open Sans", 14}, "wifi"};
   tl.flow(1000, false);               // wide: everything fits on one line

   // The line must be flushed: non-empty text always produces at least one line.
   CHECK(tl.num_lines() == 1);

   // The flowed line must have positive extent (the text is actually laid out,
   // not collapsed to nothing): the end-of-text caret sits to the right of the
   // start-of-text caret.
   auto start = tl.caret_point(0);
   auto end   = tl.caret_point(tl.text().size());
   CHECK(end.x > start.x);
   CHECK(start.y == end.y);            // single line
}

TEST_CASE("Ligature before a hard line break")
{
   // Issue cycfi/elements#384 (follow-up): a line ENDING in a ligature that is
   // immediately followed by a hard line break (e.g. "...ffl\n") must consume
   // the newline glyph, not draw it.  The end-of-text guard for trailing
   // ligatures must not also keep a trailing newline glyph, which renders as a
   // .notdef box at the right edge of the line.
   //
   // Detect it by rendering and scanning pixels: the first line of "Affl\nB"
   // must not extend further right than "Affl" drawn alone.  A leftover newline
   // box would push the rightmost inked pixel well past the ligature.

   auto rightmost_ink_first_line =
      [](char const* str) -> int
      {
         image img{300, 120};
         {
            offscreen_image offscr{img};
            canvas cnv{offscr.context()};
            cnv.add_rect(0, 0, 300, 120);
            cnv.fill_style(colors::white);
            cnv.fill();
            text_layout tl{font_descr{"Open Sans", 40}, str};
            tl.flow(290, false);
            tl.draw(cnv, {5, 45}, colors::black);
         }
         auto sz = img.bitmap_size();
         auto const* px = img.pixels();
         int w = int(sz.x), band = std::min(int(sz.y), 50);  // first line only
         int rightmost = -1;
         for (int y = 0; y != band; ++y)
            for (int x = 0; x != w; ++x)
            {
               // Any backend: white background is the max value in every
               // channel; inked (black) text lowers them.  Treat a clearly
               // non-white pixel as ink.
               auto p = px[y * w + x];
               unsigned b0 =  p        & 0xff;
               unsigned b1 = (p >>  8) & 0xff;
               unsigned b2 = (p >> 16) & 0xff;
               if (b0 < 200 || b1 < 200 || b2 < 200)
                  rightmost = std::max(rightmost, x);
            }
         return rightmost;
      };

   // The trailing newline must be the LAST glyph for the bug to bite (the user
   // pressed Return at the very end).  "Affl\n" ends in the ffl ligature then a
   // hard break that owns the final glyph.
   int plain   = rightmost_ink_first_line("Affl");
   int with_nl = rightmost_ink_first_line("Affl\n");

   // One-sided and ink-tolerant: a kept newline box only adds ink to the RIGHT,
   // so the trailing-newline line must not reach further than "Affl" alone.
   // (Some backends render no pixels in this isolated offscreen path; then both
   // are -1 and the check is trivially satisfied.)
   CHECK(with_nl <= plain + 2);
}

TEST_CASE("Trailing newline opens an empty line")
{
   // A text ending in a hard line break (Return pressed at the very end) must
   // open an empty final line so the caret can land on it.  Otherwise the caret
   // stays on the previous line and you have to press Return twice
   // (cycfi/elements#384 follow-up).  The end-of-text caret must sit below the
   // start, on a fresh line at the left.

   text_layout one{font_descr{"Open Sans", 40}, "abc"};
   one.flow(290, false);
   text_layout two{font_descr{"Open Sans", 40}, "abc\n"};
   two.flow(290, false);

   // The trailing newline adds a line.
   CHECK(two.num_lines() == one.num_lines() + 1);

   auto start = two.caret_point(0);
   auto end   = two.caret_point(two.text().size());
   CHECK(end.y > start.y);                       // caret moved to the next line
   CHECK(end.x <= start.x + 1.0f);               // ...at the left edge
}
