/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]

   Asserts the claims of docs/modules/ROOT/pages/canvas/states.adoc and draws
   its figures. Each case names the page section it comes from.

   Every probe renders onto a transparent 100 by 100 image and samples
   pixels, so a pixel (x, y) covers [x, x+1) by [y, y+1). Sample points sit
   a pixel or two clear of an edge, away from the antialiased boundary.
=============================================================================*/
#include "test_support.hpp"

namespace
{
   struct rgba8
   {
      int r, g, b, a;
   };

   // pixels() is premultiplied B, G, R, A on every backend.
   rgba8 pixel_at(image const& img, int x, int y)
   {
      auto p = reinterpret_cast<std::uint8_t const*>(img.pixels());
      p += 4 * (y * int(img.bitmap_size().x) + x);
      return {p[2], p[1], p[0], p[3]};
   }

   bool inked(rgba8 p)     { return p.a > 200; }
   bool blank(rgba8 p)     { return p.a < 30; }
   bool reddish(rgba8 p)   { return p.r > 200 && p.g < 80 && p.b < 80; }
   bool bluish(rgba8 p)    { return p.b > 200 && p.r < 80 && p.g < 80; }

   template <typename F>
   void render(image& img, F f)
   {
      offscreen_image ctx{img};
      canvas cnv{ctx.context()};
      f(cnv);
   }

   // A donut built as two squares wound the same way. Under the winding
   // rule the hole fills; under even-odd it does not.
   void donut(canvas& cnv)
   {
      cnv.begin_path();
      cnv.move_to(10, 10); cnv.line_to(90, 10);
      cnv.line_to(90, 90); cnv.line_to(10, 90); cnv.close_path();
      cnv.move_to(30, 30); cnv.line_to(70, 30);
      cnv.line_to(70, 70); cnv.line_to(30, 70); cnv.close_path();
      cnv.fill();
   }
}

///////////////////////////////////////////////////////////////////////////////
// == Overview

TEST_CASE("canvas states: what the bundle holds", "[states]")
{
   {
      // The fill and stroke styles, the line width, and the shadow come
      // back. Each is set inside a scope and the paint after it is the
      // outer one's.
      image img{100, 100, 1};
      render(img, [](canvas& cnv)
      {
         cnv.fill_style(colors::red);
         {
            auto st = cnv.new_state();
            cnv.fill_style(colors::blue);
            cnv.shadow_style({20, 20}, 2, colors::black);
            cnv.composite_op(canvas::copy);
         }
         cnv.fill_rect(10, 10, 40, 40);
      });
#if !defined(ARTIST_RECORDING)
      CHECK(reddish(pixel_at(img, 25, 25)));    // the outer fill style
      CHECK(blank(pixel_at(img, 62, 62)));      // and no shadow under it
#endif
   }

   {
      // The transform comes back.
      image img{100, 100, 1};
      render(img, [](canvas& cnv)
      {
         cnv.fill_style(colors::red);
         {
            auto st = cnv.new_state();
            cnv.translate(50, 50);
         }
         cnv.fill_rect(10, 10, 30, 30);
      });
#if !defined(ARTIST_RECORDING)
      CHECK(inked(pixel_at(img, 25, 25)));      // where the untranslated
      CHECK(blank(pixel_at(img, 75, 75)));      // call puts it
#endif
   }

   {
      // The clip comes back, which is the only way to widen it again.
      image img{100, 100, 1};
      render(img, [](canvas& cnv)
      {
         cnv.fill_style(colors::red);
         {
            auto st = cnv.new_state();
            cnv.add_rect(0, 0, 20, 20);
            cnv.clip();
         }
         cnv.fill_rect(60, 60, 30, 30);
      });
#if !defined(ARTIST_RECORDING)
      CHECK(inked(pixel_at(img, 75, 75)));
#endif
   }

   {
      // The stroke style, the line width and the line cap come back.
      image img{100, 100, 1};
      render(img, [](canvas& cnv)
      {
         cnv.stroke_style(colors::red);
         cnv.line_width(4);
         cnv.line_cap(canvas::butt);
         {
            auto st = cnv.new_state();
            cnv.stroke_style(colors::blue);
            cnv.line_width(24);
            cnv.line_cap(canvas::square);
         }
         cnv.move_to(30, 50);
         cnv.line_to(70, 50);
         cnv.stroke();
      });
#if !defined(ARTIST_RECORDING)
      CHECK(reddish(pixel_at(img, 50, 50)));    // the outer stroke style
      CHECK(blank(pixel_at(img, 50, 60)));      // the outer line width
      CHECK(blank(pixel_at(img, 25, 50)));      // and the outer butt cap
#endif
   }

   {
      // The composite operation comes back. Under source_over a half
      // transparent blue over opaque red blends to purple; under the copy
      // set inside the scope it would replace the red outright.
      image img{100, 100, 1};
      render(img, [](canvas& cnv)
      {
         cnv.fill_style(colors::red);
         cnv.fill_rect(10, 10, 60, 60);
         {
            auto st = cnv.new_state();
            cnv.composite_op(canvas::copy);
         }
         cnv.fill_style(colors::blue.opacity(0.5));
         cnv.fill_rect(10, 10, 60, 60);
      });
#if !defined(ARTIST_RECORDING)
      auto p = pixel_at(img, 40, 40);
      CHECK(p.r > 80);                          // the red is still under it
      CHECK(p.b > 80);
#endif
   }

   {
      // The font comes back.
      image img{200, 60, 1};
      render(img, [](canvas& cnv)
      {
         cnv.font(font_descr{"Open Sans", 12});
         auto before = cnv.measure_text("Hamburg");
         {
            auto st = cnv.new_state();
            cnv.font(font_descr{"Open Sans", 40});
         }
         auto after = cnv.measure_text("Hamburg");
         CHECK(after.size.x == Approx(before.size.x));
         CHECK(after.ascent == Approx(before.ascent));
      });
   }

   {
      // And so does the text alignment. Left aligned, the ink starts at
      // the point given; centred, it would straddle it.
      image img{200, 60, 1};
      render(img, [](canvas& cnv)
      {
         cnv.fill_style(colors::red);
         cnv.font(font_descr{"Open Sans", 24});
         cnv.text_align(canvas::left | canvas::baseline);
         {
            auto st = cnv.new_state();
            cnv.text_align(canvas::center | canvas::baseline);
         }
         cnv.fill_text("HH", 100, 40);
      });
#if !defined(ARTIST_RECORDING)
      int left_ink = 0, right_ink = 0;
      for (int y = 0; y != 60; ++y)
         for (int x = 0; x != 200; ++x)
            if (!blank(pixel_at(img, x, y)))
               (x < 100? left_ink : right_ink)++;
      CHECK(left_ink == 0);
      CHECK(right_ink > 100);
#endif
   }
}

TEST_CASE("canvas states: what the bundle does not hold", "[states]")
{
   {
      // The current path is not state. A shape built before a save is
      // still there after the restore, and still in its own coordinates:
      // the points were placed when they were added.
      image img{100, 100, 1};
      render(img, [](canvas& cnv)
      {
         cnv.fill_style(colors::red);
         cnv.add_rect(10, 10, 40, 40);
         {
            auto st = cnv.new_state();
            cnv.translate(50, 50);
         }
         cnv.fill();
      });
#if !defined(ARTIST_RECORDING)
      CHECK(inked(pixel_at(img, 25, 25)));
      CHECK(blank(pixel_at(img, 75, 75)));
#endif
   }

   {
      // Nor is a shape built inside a scope rolled back by the restore.
      image img{100, 100, 1};
      render(img, [](canvas& cnv)
      {
         cnv.fill_style(colors::red);
         {
            auto st = cnv.new_state();
            cnv.add_rect(10, 10, 40, 40);
         }
         cnv.fill();
      });
#if !defined(ARTIST_RECORDING)
      CHECK(inked(pixel_at(img, 25, 25)));
#endif
   }

   {
      // Ink is not state either: a restore is not an undo. What was
      // painted inside the scope stays painted.
      image img{100, 100, 1};
      render(img, [](canvas& cnv)
      {
         auto st = cnv.new_state();
         cnv.fill_style(colors::blue);
         cnv.fill_rect(10, 10, 40, 40);
      });
#if !defined(ARTIST_RECORDING)
      CHECK(inked(pixel_at(img, 25, 25)));
#endif
   }
}

///////////////////////////////////////////////////////////////////////////////
// == Expressions -> === Saving and Restoring

TEST_CASE("canvas states: the stack nests", "[states]")
{
   // Three levels, one fill style each. The paints on the way out get the
   // levels back in reverse order.
   image img{160, 40, 1};
   render(img, [](canvas& cnv)
   {
      cnv.fill_style(colors::red);
      {
         auto outer = cnv.new_state();
         cnv.fill_style(colors::green);
         {
            auto inner = cnv.new_state();
            cnv.fill_style(colors::blue);
            cnv.fill_rect(0, 0, 40, 40);
         }
         cnv.fill_rect(40, 0, 40, 40);      // green again
      }
      cnv.fill_rect(80, 0, 40, 40);         // red again
   });
#if !defined(ARTIST_RECORDING)
   CHECK(bluish(pixel_at(img, 20, 20)));
   auto mid = pixel_at(img, 60, 20);
   CHECK(mid.g > 200);
   CHECK(mid.r < 80);
   CHECK(reddish(pixel_at(img, 100, 20)));
#endif
}

TEST_CASE("canvas states: save and restore by hand", "[states]")
{
   // cnv.save() and cnv.restore() are what new_state() calls. The same
   // drawing done both ways paints the same pixels.
   auto draw = [](canvas& cnv, bool by_hand)
   {
      cnv.fill_style(colors::red);
      if (by_hand)
      {
         cnv.save();
         cnv.fill_style(colors::blue);
         cnv.translate(30, 30);
         cnv.restore();
      }
      else
      {
         auto st = cnv.new_state();
         cnv.fill_style(colors::blue);
         cnv.translate(30, 30);
      }
      cnv.fill_rect(10, 10, 40, 40);
   };

   image one{100, 100, 1};
   render(one, [&](canvas& cnv) { draw(cnv, true); });
   image two{100, 100, 1};
   render(two, [&](canvas& cnv) { draw(cnv, false); });

#if !defined(ARTIST_RECORDING)
   for (int y = 0; y < 100; y += 7)
      for (int x = 0; x < 100; x += 7)
         REQUIRE(pixel_at(one, x, y).a == pixel_at(two, x, y).a);
   CHECK(reddish(pixel_at(one, 25, 25)));
#endif
}

TEST_CASE("canvas states: a clip only ever narrows", "[states]")
{
   // Nesting a clip intersects it with the one already in effect, and the
   // restore puts the wider one back.
   {
      image img{100, 100, 1};
      render(img, [](canvas& cnv)
      {
         cnv.add_rect(0, 0, 50, 50);
         cnv.clip();
         auto outer = cnv.clip_extent();
         CHECK(outer.right == Approx(50));
         CHECK(outer.bottom == Approx(50));
         {
            auto st = cnv.new_state();
            cnv.add_rect(40, 40, 100, 100);   // reaches past the outer clip
            cnv.clip();
            auto nested = cnv.clip_extent();
            CHECK(nested.left == Approx(40));
            CHECK(nested.top == Approx(40));
            CHECK(nested.right == Approx(50));
            CHECK(nested.bottom == Approx(50));
         }
         auto back = cnv.clip_extent();
         CHECK(back.left == Approx(outer.left));
         CHECK(back.top == Approx(outer.top));
         CHECK(back.right == Approx(outer.right));
         CHECK(back.bottom == Approx(outer.bottom));
      });
   }
}

///////////////////////////////////////////////////////////////////////////////
// == Expressions -> === Constructors and Assignment

TEST_CASE("canvas states: the state object", "[states]")
{
   {
      // An exception leaving the block restores the state, which is the
      // reason to prefer new_state() to a hand written pair.
      image img{100, 100, 1};
      render(img, [](canvas& cnv)
      {
         cnv.fill_style(colors::red);
         try
         {
            auto st = cnv.new_state();
            cnv.fill_style(colors::blue);
            throw std::runtime_error("unwound");
         }
         catch (std::exception const&) {}
         cnv.fill_rect(10, 10, 40, 40);
      });
#if !defined(ARTIST_RECORDING)
      CHECK(reddish(pixel_at(img, 25, 25)));
#endif
   }

   {
      // Moving a state hands the save over. The moved-from object restores
      // nothing, so the pair stays balanced.
      image img{100, 100, 1};
      render(img, [](canvas& cnv)
      {
         cnv.fill_style(colors::red);
         {
            auto a = cnv.new_state();
            cnv.fill_style(colors::blue);
            auto b = std::move(a);
         }
         cnv.fill_rect(10, 10, 40, 40);
      });
#if !defined(ARTIST_RECORDING)
      CHECK(reddish(pixel_at(img, 25, 25)));
#endif
   }

   static_assert(!std::is_copy_constructible_v<canvas::state>);
   static_assert(!std::is_copy_assignable_v<canvas::state>);
   static_assert(std::is_move_constructible_v<canvas::state>);
   static_assert(std::is_move_assignable_v<canvas::state>);
}

///////////////////////////////////////////////////////////////////////////////
// Behaviour under review. These pin down what the library does today so a
// change is visible. None of them asserts the behaviour is correct.

TEST_CASE("canvas states: current behaviour, fill_rule", "[states]")
{
   // The fill rule is part of the saved bundle on Cairo and not on
   // Quartz 2D, where canvas_state::_fill_rule sits outside the stack.
   // Setting it inside a scope leaks out of the scope there.
   image img{100, 100, 1};
   render(img, [](canvas& cnv)
   {
      cnv.fill_style(colors::red);
      cnv.fill_rule(path::fill_winding);
      {
         auto st = cnv.new_state();
         cnv.fill_rule(path::fill_odd_even);
      }
      donut(cnv);
   });

#if defined(ARTIST_CAIRO)
   // Restored: the winding rule fills the hole.
   CHECK(inked(pixel_at(img, 50, 50)));
#elif defined(ARTIST_QUARTZ_2D)
   // Not restored: the even-odd rule set inside the scope still applies.
   CHECK(blank(pixel_at(img, 50, 50)));
#endif
}

TEST_CASE("canvas states: current behaviour, move assignment", "[states]")
{
   // canvas::state::operator=(state&&) overwrites its canvas pointer
   // without restoring first, so assigning over a live state drops that
   // state's save and the stack is left one level deep.
   image img{100, 100, 1};
   render(img, [](canvas& cnv)
   {
      cnv.fill_style(colors::red);
      {
         auto a = cnv.new_state();           // save 1
         cnv.fill_style(colors::green);
         {
            auto b = cnv.new_state();        // save 2
            cnv.fill_style(colors::blue);
            a = std::move(b);                // save 1 dropped, not restored
         }
      }                                      // a restores save 2 only
      cnv.fill_rect(10, 10, 40, 40);
   });

#if !defined(ARTIST_RECORDING)
   // Green, not red: one level of the stack is still on it.
   auto p = pixel_at(img, 25, 25);
   CHECK(p.g > 200);
   CHECK(p.r < 80);
#endif
}

TEST_CASE("canvas states: current behaviour, unmatched restore", "[states]")
{
   // A restore with no matching save is a no-op in the W3C API. Artist
   // has three answers. On Quartz 2D and Skia canvas_state::restore()
   // pops the last entry and the next access to it is a read of an empty
   // stack, which crashes; those two are not exercised here for that
   // reason. On Cairo cairo_restore is called all the same and the
   // context goes into a permanent error state, so nothing drawn
   // afterwards lands. Only the recording and Direct2D backends guard the
   // last entry and do nothing, which is the W3C behaviour.
#if defined(ARTIST_CAIRO)
   image img{100, 100, 1};
   render(img, [](canvas& cnv)
   {
      cnv.restore();                         // one too many
      cnv.fill_style(colors::red);
      cnv.fill_rect(10, 10, 40, 40);
   });
   CHECK(blank(pixel_at(img, 25, 25)));      // the context is dead
#endif
#if defined(ARTIST_RECORDING)
   image img{100, 100, 1};
   render(img, [](canvas& cnv)
   {
      cnv.restore();
      cnv.fill_style(colors::red);
      cnv.fill_rect(10, 10, 40, 40);
   });
   CHECK(recorded(img).count(recording::op::fill) == 1);
#endif
}

///////////////////////////////////////////////////////////////////////////////
// The page figures.

namespace
{
   color const slate = rgba(93, 93, 93, 255);      // #5d5d5d
   color const ink = rgba(26, 26, 26, 255);        // #1a1a1a
   color const accent = rgba(21, 101, 192, 255);   // #1565c0
   color const amber = rgba(255, 179, 0, 255);     // #ffb300

   void caption(canvas& cnv, char const* s, float x, float y)
   {
      auto st = cnv.new_state();
      cnv.font(font_descr{"Open Sans", 15});
      cnv.fill_style(ink);
      cnv.text_align(canvas::center | canvas::baseline);
      cnv.fill_text(s, x, y);
   }

   // A white figure of the page's width, rendered by f and saved as name.
   template <typename F>
   void figure(float h, char const* name, F f)
   {
      float const w = 560;
      image img{w, h, 2};
      {
         offscreen_image ctx{img};
         canvas cnv{ctx.context()};
         cnv.fill_style(colors::white);
         cnv.fill_rect(0, 0, w, h);
         f(cnv);
      }
      img.save_png(get_results_path() + name);
   }
}

TEST_CASE("canvas states: stack figure", "[states]")
{
   // The page figure images/canvas/state_stack.png: five swatches painted
   // by the same statement at five points in a nested save and restore.
   // The colour is the current fill style, so the swatches read the stack
   // out loud: what goes on comes back off in reverse.
   figure(190, "canvas_state_stack.png", [](canvas& cnv)
   {
      float const x0 = 30, step = 104, w = 76;

      auto swatch = [&](int i)
      {
         cnv.fill_round_rect(rect{x0 + i * step, 30, x0 + i * step + w, 106}, 6);
      };

      cnv.fill_style(accent);
      swatch(0);
      {
         auto outer = cnv.new_state();
         cnv.fill_style(amber);
         swatch(1);
         {
            auto inner = cnv.new_state();
            cnv.fill_style(slate);
            swatch(2);
         }
         swatch(3);
      }
      swatch(4);

      char const* labels[] =
      {
         "base", "save", "save", "restore", "restore"
      };
      for (int i = 0; i != 5; ++i)
         caption(cnv, labels[i], x0 + i * step + w / 2, 132);

      // The depth the stack is at under each swatch.
      char const* depth[] = {"0", "1", "2", "1", "0"};
      for (int i = 0; i != 5; ++i)
         caption(cnv, depth[i], x0 + i * step + w / 2, 162);
      caption(cnv, "depth", 30, 162);
   });
}

namespace
{
   // The page's == Example, verbatim, so the page's code is the code that
   // draws the page's figure and the code the assertions below check.
   void example(canvas& cnv)
   {
      auto panel = rect{20, 20, 260, 120};

      cnv.fill_style(rgba(21, 101, 192, 255));
      cnv.fill_round_rect(panel, 8);

      {
         auto st = cnv.new_state();
         cnv.add_round_rect(panel, 8);
         cnv.clip();
         cnv.translate(panel.left + 20, panel.bottom - 26);
         cnv.rotate(-0.35);
         cnv.font(font_descr{"Open Sans", 34});
         cnv.fill_style(colors::white);
         cnv.fill_text("Artist Artist Artist", 0, 0);
      }

      // The clip, the transform, the font and the fill style are all back:
      // this lands outside the panel, unclipped and unrotated.
      cnv.fill_style(rgba(26, 26, 26, 255));
      cnv.fill_rect(280, 60, 40, 40);
   }
}

TEST_CASE("canvas states: Example", "[states]")
{
   image img{340, 140, 1};
   render(img, [](canvas& cnv) { example(cnv); });

#if !defined(ARTIST_RECORDING)
   // The panel is painted in the fill style set before the scope.
   auto panel_ink = pixel_at(img, 140, 40);
   CHECK(panel_ink.b > 150);
   CHECK(panel_ink.r < 60);

   // The label is white and runs off the panel's corner, so the clip set
   // inside the scope is what decides where it stops: white ink inside the
   // panel, and not one white pixel outside it.
   auto is_white = [](rgba8 p)
   {
      return p.a > 200 && p.r > 200 && p.g > 200 && p.b > 200;
   };
   int white_in = 0, white_out = 0;
   for (int y = 0; y != 140; ++y)
      for (int x = 0; x != 340; ++x)
      {
         if (!is_white(pixel_at(img, x, y)))
            continue;
         if (x > 24 && x < 256 && y > 24 && y < 116)
            ++white_in;
         else if (x < 17 || x > 263 || y < 17 || y > 123)
            ++white_out;        // clear of the panel and its antialiasing
      }
   CHECK(white_in > 500);
   CHECK(white_out == 0);

   // The square after the scope lands outside the panel, so the clip is
   // gone, and it is square to the axes and dark, so the transform and the
   // fill style came back too.
   auto mark = pixel_at(img, 300, 80);
   CHECK(inked(mark));
   CHECK(mark.r < 60);
   CHECK(mark.g < 60);
   CHECK(mark.b < 60);
   CHECK(inked(pixel_at(img, 282, 62)));    // its top left corner is a corner
   CHECK(blank(pixel_at(img, 278, 80)));    // and nothing just left of it
#endif
}

TEST_CASE("canvas states: example figure", "[states]")
{
   // The page figure images/canvas/states_example.png: what the code under
   // == Example draws, shifted right to sit centred in the figure's width.
   figure(140, "canvas_states_example.png", [](canvas& cnv)
   {
      cnv.translate(110, 0);
      example(cnv);
   });
}
