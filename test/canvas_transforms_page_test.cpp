/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]

   Asserts the claims of docs/modules/ROOT/pages/canvas/transforms.adoc. Each
   case names the page section it comes from.
=============================================================================*/
#include "test_support.hpp"
#include <numbers>

namespace
{
   // Run f on a canvas over an image of the given scale.
   template <typename F>
   void on_canvas(F f, float scale = 1)
   {
      image img{100, 50, scale};
      offscreen_image ctx{img};
      canvas cnv{ctx.context()};
      f(cnv);
   }

   bool near(point a, point b, float tol = 0.01f)
   {
      return std::abs(a.x - b.x) <= tol && std::abs(a.y - b.y) <= tol;
   }
}

TEST_CASE("canvas transforms: Composition", "[transforms]")
{
   on_canvas([](canvas& cnv)
   {
      // Each call composes on top of the one before, so the last call is the
      // one nearest the drawing: scale by 2, then translate by 10, and the
      // move is 20 device units.
      cnv.scale(2, 2);
      cnv.translate(10, 0);
      CHECK(near(cnv.user_to_device(point{0, 0}), {20, 0}));
   });

   // Rotation is about the origin of user space, and a positive angle turns
   // clockwise on screen because y grows downward.
   on_canvas([](canvas& cnv)
   {
      cnv.rotate(float(std::numbers::pi) / 2);
      CHECK(near(cnv.user_to_device(point{1, 0}), {0, 1}));
   });

   on_canvas([](canvas& cnv)
   {
      cnv.scale(3);                            // both axes
      CHECK(near(cnv.user_to_device(point{1, 2}), {3, 6}));
   });

   // The transform is part of the saved state.
   on_canvas([](canvas& cnv)
   {
      {
         auto s = cnv.new_state();
         cnv.scale(2, 2);
         CHECK(near(cnv.user_to_device(point{1, 1}), {2, 2}));
      }
      CHECK(near(cnv.user_to_device(point{1, 1}), {1, 1}));
   });
}

TEST_CASE("canvas transforms: skew takes angles", "[transforms]")
{
   // sx is the angle by which y shears with x, sy the angle by which x
   // shears with y, as for affine_transform::skew.
   auto t = float(std::tan(0.5));

   on_canvas([&](canvas& cnv)
   {
      cnv.skew(0.5, 0);
      CHECK(near(cnv.user_to_device(point{1, 0}), {1, t}));
      CHECK(near(cnv.user_to_device(point{0, 1}), {0, 1}));
   });

   on_canvas([&](canvas& cnv)
   {
      cnv.skew(0, 0.5);
      CHECK(near(cnv.user_to_device(point{0, 1}), {t, 1}));
      CHECK(near(cnv.user_to_device(point{1, 0}), {1, 0}));
   });
}

TEST_CASE("canvas transforms: Conversion", "[transforms]")
{
   // user_to_device and device_to_user are inverses, and both ignore the
   // transform the canvas started with: over an image at scale 2, a user
   // space point still lands on itself.
   on_canvas([](canvas& cnv)
   {
      CHECK(near(cnv.user_to_device(point{1, 1}), {1, 1}));
      cnv.scale(2, 2);
      auto d = cnv.user_to_device(point{3, 4});
      CHECK(near(d, {6, 8}));
      CHECK(near(cnv.device_to_user(d), {3, 4}));
      CHECK(near(cnv.device_to_user(d.x, d.y), cnv.device_to_user(d)));
   }, 2);
}

TEST_CASE("canvas transforms: The Matrix", "[transforms]")
{
   // transform() reads the backend's own matrix and transform(mat) puts one
   // back: a matrix from the same canvas restores what it described.
   on_canvas([](canvas& cnv)
   {
      auto start = cnv.transform();
      cnv.scale(4, 4);
      CHECK(near(cnv.user_to_device(point{1, 1}), {4, 4}));

      cnv.transform(start);
      CHECK(near(cnv.user_to_device(point{1, 1}), {1, 1}));

      cnv.transform(start.a, start.b, start.c, start.d, start.tx, start.ty);
      CHECK(near(cnv.user_to_device(point{1, 1}), {1, 1}));
   }, 2);
}

namespace
{
   color const ghost = rgba(176, 176, 176, 255);   // #b0b0b0
   color const accent = rgba(21, 101, 192, 255);   // #1565c0

   // The square every panel transforms, in panel-local units.
   void square(canvas& cnv)
   {
      cnv.add_rect(15, 15, 55, 55);
   }
}

TEST_CASE("canvas transforms: figure", "[transforms]")
{
   // The page figure images/canvas/transforms.png: the same square under
   // each composing transform, about the panel's own origin.
   float const w = 560, h = 200;
   image img{w, h, 2};
   {
      offscreen_image ctx{img};
      canvas cnv{ctx.context()};
      cnv.fill_style(colors::white);
      cnv.fill_rect(0, 0, w, h);

      char const* names[] =
      {
         "translate(20, 10)", "rotate(pi / 6)", "scale(1.3, 0.7)", "skew(0.3, 0)"
      };

      for (int i = 0; i != 4; ++i)
      {
         auto panel = point{40.0f + i * 130.0f, 45.0f};
         {
            auto state = cnv.new_state();
            cnv.translate(panel.x, panel.y);

            // Where the square lands with no transform.
            cnv.stroke_style(ghost);
            cnv.line_width(1.25);
            square(cnv);
            cnv.stroke();

            switch (i)
            {
               case 0: cnv.translate(20, 10); break;
               case 1: cnv.rotate(float(std::numbers::pi) / 6); break;
               case 2: cnv.scale(1.3f, 0.7f); break;
               case 3: cnv.skew(0.3, 0); break;
            }

            cnv.fill_style(accent.opacity(0.25f));
            cnv.stroke_style(accent);
            cnv.line_width(2);
            square(cnv);
            cnv.fill_preserve();
            cnv.stroke();
         }

         cnv.font(font_descr{"Open Sans", 15});
         cnv.fill_style(colors::black);
         cnv.text_align(canvas::center | canvas::baseline);
         cnv.fill_text(names[i], panel.x + 30, 175);
      }
   }
   img.save_png(get_results_path() + "canvas_transforms.png");
}
