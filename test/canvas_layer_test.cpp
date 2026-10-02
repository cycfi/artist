/*=============================================================================
   Copyright (c) 2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]

   Asserts the claims of docs/modules/ROOT/pages/foundation/canvas_layer.adoc
   and draws its figure. Each case names the page section it comes from.

   Every probe renders onto an image and samples pixels, so a pixel (x, y)
   covers [x, x+1) by [y, y+1). Sample points sit a pixel or two clear of an
   edge, away from the antialiased boundary.
=============================================================================*/
#include "test_support.hpp"
#include <artist/canvas_layer.hpp>

namespace
{
   struct rgba8
   {
      int r, g, b, a;
   };

   bool near(rgba8 x, rgba8 y, int tol = 3)
   {
      return std::abs(x.r - y.r) <= tol && std::abs(x.g - y.g) <= tol
         && std::abs(x.b - y.b) <= tol && std::abs(x.a - y.a) <= tol;
   }

   // pixels() is premultiplied B, G, R, A on every backend.
   rgba8 pixel_at(image const& img, int x, int y)
   {
      auto p = reinterpret_cast<std::uint8_t const*>(img.pixels());
      p += 4 * (y * int(img.bitmap_size().x) + x);
      return {p[2], p[1], p[0], p[3]};
   }

   bool clear(rgba8 p)     { return p.a < 30; }
   bool is_red(rgba8 p)    { return p.r > 200 && p.g < 60 && p.b < 60; }
   bool is_green(rgba8 p)  { return p.g > 200 && p.r < 60 && p.b < 60; }

   auto constexpr red = rgba(255, 0, 0, 255);
   auto constexpr blue = rgba(0, 0, 255, 255);
   auto constexpr lime = rgba(0, 255, 0, 255);

   template <typename F>
   void render(image& img, F f)
   {
      offscreen_image ctx{img};
      canvas cnv{ctx.context()};
      f(cnv);
   }

   // Fill the whole layer with one colour.
   void fill_layer(canvas_layer& layer, color c)
   {
      canvas lc{layer.context()};
      lc.fill_style(c);
      lc.fill_rect(rect{0, 0, layer.size().x, layer.size().y});
   }
}

TEST_CASE("canvas_layer: Overview", "[canvas_layer]")
{
   {
      // A layer starts fully transparent: drawing a fresh one paints nothing.
      image dst{extent{40, 40}};
      render(dst, [](canvas& cnv)
      {
         cnv.fill_style(lime);
         cnv.fill_rect(rect{0, 0, 40, 40});
         canvas_layer layer{cnv, extent{40, 40}};
         cnv.draw(layer);
      });

#if !defined(ARTIST_RECORDING)
      CHECK(is_green(pixel_at(dst, 20, 20)));
#endif
   }

   {
      // What is drawn into a layer is what drawing the layer puts down.
      image dst{extent{100, 100}};
      render(dst, [](canvas& cnv)
      {
         canvas_layer layer{cnv, extent{50, 50}};
         CHECK(layer.size().x == 50);
         CHECK(layer.size().y == 50);
         fill_layer(layer, red);
         cnv.draw(layer, point{25, 25});
      });

#if defined(ARTIST_RECORDING)
      REQUIRE(recorded(dst).size() == 1);
      CHECK(recorded(dst, 0).kind == recording::op::image);
#else
      CHECK(near(pixel_at(dst, 50, 50), {255, 0, 0, 255}));
      CHECK(clear(pixel_at(dst, 10, 10)));
      CHECK(clear(pixel_at(dst, 90, 90)));
#endif
   }

   {
      // Nothing is cleared between one draw and the next: a layer keeps what
      // was drawn into it, and later drawing lands on top of it.
      image dst{extent{60, 60}};
      render(dst, [](canvas& cnv)
      {
         canvas_layer layer{cnv, extent{60, 60}};
         {
            canvas lc{layer.context()};
            lc.fill_style(blue);
            lc.fill_rect(rect{0, 0, 30, 60});
         }
         cnv.draw(layer);
         {
            canvas lc{layer.context()};
            lc.fill_style(lime);
            lc.fill_rect(rect{30, 0, 60, 60});
         }
         cnv.draw(layer);
      });

#if defined(ARTIST_RECORDING)
      REQUIRE(recorded(dst).size() == 2);
      CHECK(recorded(dst, 1).kind == recording::op::image);
#else
      CHECK(near(pixel_at(dst, 15, 30), {0, 0, 255, 255}));
      CHECK(near(pixel_at(dst, 45, 30), {0, 255, 0, 255}));
#endif
   }

   {
      // clear_rect on the layer's own canvas puts a region back to
      // transparent, and the green ground shows through where it did.
      image dst{extent{40, 40}};
      render(dst, [](canvas& cnv)
      {
         cnv.fill_style(lime);
         cnv.fill_rect(rect{0, 0, 40, 40});
         canvas_layer layer{cnv, extent{40, 40}};
         fill_layer(layer, red);
         {
            canvas lc{layer.context()};
            lc.clear_rect(rect{0, 0, 20, 40});
         }
         cnv.draw(layer);
      });

#if !defined(ARTIST_RECORDING)
      CHECK(is_green(pixel_at(dst, 10, 20)));
      CHECK(is_red(pixel_at(dst, 30, 20)));
#endif
   }

   {
      // The layer's canvas draws in the layer's own units, from the layer's
      // top left corner, with y running down.
      image dst{extent{40, 40}};
      render(dst, [](canvas& cnv)
      {
         canvas_layer layer{cnv, extent{40, 40}};
         {
            canvas lc{layer.context()};
            lc.fill_style(red);
            lc.fill_rect(rect{0, 0, 10, 4});    // a bar along the top edge
         }
         cnv.draw(layer);
      });

#if !defined(ARTIST_RECORDING)
      CHECK(is_red(pixel_at(dst, 5, 2)));
      CHECK(clear(pixel_at(dst, 5, 37)));
#endif
   }

   {
      // Drawing a layer is drawing, so the current transform carries it and
      // the clip cuts it; the current path is neither painted nor disturbed.
      image dst{extent{80, 80}};
      render(dst, [](canvas& cnv)
      {
         canvas_layer layer{cnv, extent{20, 20}};
         fill_layer(layer, red);

         cnv.fill_style(lime);
         cnv.add_rect(rect{0, 0, 10, 10});      // built, not painted
         {
            auto st = cnv.new_state();
            cnv.translate(40, 40);
            cnv.draw(layer, point{0, 0});
         }
         cnv.fill();                            // still there to paint
      });

#if !defined(ARTIST_RECORDING)
      CHECK(is_red(pixel_at(dst, 50, 50)));     // the transform carried it
      CHECK(is_green(pixel_at(dst, 5, 5)));     // the path survived
#endif
   }

   {
      // The clip cuts a layer as it cuts any other paint.
      image dst{extent{40, 40}};
      render(dst, [](canvas& cnv)
      {
         canvas_layer layer{cnv, extent{40, 40}};
         fill_layer(layer, red);
         auto st = cnv.new_state();
         cnv.add_rect(rect{0, 0, 20, 40});
         cnv.clip();
         cnv.draw(layer);
      });

#if !defined(ARTIST_RECORDING)
      CHECK(is_red(pixel_at(dst, 10, 20)));
      CHECK(clear(pixel_at(dst, 30, 20)));
#endif
   }
}

TEST_CASE("canvas_layer: Constructors and Assignment", "[canvas_layer]")
{
   // A move carries the surface and its contents; the layer is usable in its
   // new home and reports the same size.
   image dst{extent{80, 80}};
   render(dst, [](canvas& cnv)
   {
      canvas_layer layer{cnv, extent{20, 20}};
      fill_layer(layer, red);

      canvas_layer moved{std::move(layer)};
      CHECK(moved.size().x == 20);

      canvas_layer target{cnv, extent{4, 4}};
      target = std::move(moved);
      CHECK(target.size().x == 20);
      CHECK(target.size().y == 20);

      cnv.draw(target, rect{0, 0, 80, 80});     // stretched to fill dest
   });

#if !defined(ARTIST_RECORDING)
   CHECK(is_red(pixel_at(dst, 70, 70)));
#endif
}

TEST_CASE("canvas_layer: Accessors", "[canvas_layer]")
{
   {
      // size() is in units, not pixels: on a 2x canvas a layer covers the
      // same ground and the density goes into the detail. Half a unit of
      // fill lands as one whole opaque device pixel, which it could not do
      // if the layer were rasterised at one pixel per unit.
      image dst{extent{20, 20}, 2};
      render(dst, [](canvas& cnv)
      {
         canvas_layer layer{cnv, extent{20, 20}};
         CHECK(layer.size().x == 20);
         {
            canvas lc{layer.context()};
            lc.fill_style(red);
            lc.fill_rect(rect{0, 0, 0.5f, 20});
         }
         cnv.draw(layer, point{0, 0});
      });

#if !defined(ARTIST_RECORDING)
      CHECK(dst.bitmap_size().x == 40);
      CHECK(near(pixel_at(dst, 0, 10), {255, 0, 0, 255}));
      CHECK(clear(pixel_at(dst, 1, 10)));
#endif
   }

   {
      // The density also follows the scale in force on the canvas when the
      // layer is constructed.
      image dst{extent{40, 40}, 1};
      render(dst, [](canvas& cnv)
      {
         cnv.scale(2, 2);                       // one canvas unit is two px
         canvas_layer layer{cnv, extent{10, 10}};
         {
            canvas lc{layer.context()};
            lc.fill_style(red);
            lc.fill_rect(rect{0, 0, 0.5f, 10});
         }
         cnv.draw(layer, point{0, 0});
      });

#if !defined(ARTIST_RECORDING)
      CHECK(near(pixel_at(dst, 0, 5), {255, 0, 0, 255}));
      CHECK(clear(pixel_at(dst, 1, 5)));
#endif
   }

   {
      // context() and impl() hand out the backend's own handles.
      image dst{extent{20, 20}};
      render(dst, [](canvas& cnv)
      {
         canvas_layer layer{cnv, extent{10, 10}};
         CHECK(layer.context() != nullptr);
         CHECK(layer.impl() != nullptr);
      });
   }
}

///////////////////////////////////////////////////////////////////////////////
// The page figure.

namespace
{
   // The == Example code verbatim, so the code on the page is the code that
   // draws the page's figure.
   void layer_example(canvas& cnv)
   {
      // The grid is drawn once, into the layer, and stays there.
      canvas_layer trace{cnv, extent{520, 120}};
      {
         canvas lc{trace.context()};
         lc.stroke_style(colors::dim_gray);
         lc.line_width(1);
         for (float x = 65; x < 520; x += 65)
         {
            lc.move_to({x, 0});
            lc.line_to({x, 120});
         }
         lc.move_to({0, 60});
         lc.line_to({520, 60});
         lc.stroke();
      }

      auto y_at = [](float x)
      {
         return 60 - 48 * std::exp(-x / 220) * std::sin(x * 0.05f);
      };

      // One frame: extend the trace, then draw the layer and the head.
      auto frame = [&](float from, float to)
      {
         {
            canvas lc{trace.context()};
            lc.stroke_style(colors::light_sea_green);
            lc.line_width(2);
            lc.move_to({from, y_at(from)});
            for (float x = from + 2; x <= to; x += 2)
               lc.line_to({x, y_at(x)});
            lc.stroke();
         }

         cnv.fill_style(colors::gray[8]);
         cnv.fill_rect(rect{20, 30, 540, 150});
         cnv.draw(trace, point{20, 30});
         cnv.fill_style(colors::white);
         cnv.add_circle(20 + to, 30 + y_at(to), 4);
         cnv.fill();
      };

      for (float x = 0; x < 480; x += 40)
         frame(x, x + 40);
   }
}

TEST_CASE("canvas_layer: example figure", "[canvas_layer]")
{
   // The page figure images/foundation/canvas_layer_example.png: what the
   // code under == Example draws.
   float const w = 560, h = 180;
   image img{w, h, 2};
   {
      offscreen_image ctx{img};
      canvas cnv{ctx.context()};
      cnv.fill_style(colors::white);
      cnv.fill_rect(0, 0, w, h);
      layer_example(cnv);
   }
   img.save_png(get_results_path() + "canvas_layer_example.png");

#if !defined(ARTIST_RECORDING)
   // The head of the trace, white, at the end of the twelfth frame.
   auto head = pixel_at(img, 2 * 500, 2 * 95);
   CHECK((head.r > 200 && head.g > 200 && head.b > 200));
#endif
}

///////////////////////////////////////////////////////////////////////////////
// Behaviour under review, pinned so a change is visible. Neither case says
// the behaviour is correct.

TEST_CASE("canvas_layer: current behaviour, degenerate size", "[canvas_layer]")
{
   // A layer with a zero or negative size. Cairo builds one anyway, clamping
   // its surface to a single device pixel while size() reports what was
   // asked for. Quartz 2D throws: std::runtime_error from the bitmap context
   // for a zero size, and a std::vector length error for a negative one,
   // because image_impl allocates size_t(w) * h * 4 bytes without checking
   // the sign. The page says only to give a layer a positive size.
   image dst{extent{40, 40}};
   offscreen_image ctx{dst};
   canvas cnv{ctx.context()};

#if defined(ARTIST_CAIRO)
   canvas_layer zero{cnv, extent{0, 0}};
   CHECK(zero.size().x == 0);
   canvas_layer negative{cnv, extent{-5, 10}};
   CHECK(negative.size().x == -5);
#elif defined(ARTIST_QUARTZ_2D)
   CHECK_THROWS_AS(canvas_layer(cnv, extent{0, 0}), std::runtime_error);
   CHECK_THROWS(canvas_layer(cnv, extent{-5, 10}));
#endif
}
