/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]

   A backend may work out the shadow of a path that is one rectangle,
   rounded rectangle or circle from the shape, instead of blurring its
   pixels. That must not show, to within what the golden comparison lets
   pass. These cases hold such a shadow to that of the same outline built
   from segments, which no backend shortcuts, and a turned shape's to
   that of the same shape on its side. They also check that a path with
   more in it still casts all of its shadow.
=============================================================================*/
#include "test_support.hpp"
#include <functional>
#include <numbers>

#if !defined(ARTIST_RECORDING)

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

   template <typename F>
   void render(image& img, F f)
   {
      offscreen_image ctx{img};
      canvas cnv{ctx.context()};
      f(cnv);
   }

   using draw = std::function<void(canvas&)>;

   color const red = rgba(255, 0, 0, 255);
   color const blue = rgba(0, 0, 255, 255);
   rgba8 const red8 = {255, 0, 0, 255};
   rgba8 const blue8 = {0, 0, 255, 255};

   constexpr auto pi = std::numbers::pi_v<float>;
   constexpr rect box{20, 16, 76, 58};
   constexpr float corner = 10;

   // Each outline as a shape, and as the same outline built from segments.
   void shape_rect(canvas& cnv)
   {
      cnv.add_rect(box);
   }

   void drawn_rect(canvas& cnv)
   {
      cnv.move_to(box.top_left());
      cnv.line_to(box.top_right());
      cnv.line_to(box.bottom_right());
      cnv.line_to(box.bottom_left());
      cnv.close_path();
   }

   void shape_round_rect(canvas& cnv)
   {
      cnv.add_round_rect(box, corner);
   }

   void drawn_round_rect(canvas& cnv)
   {
      cnv.move_to(box.left + corner, box.top);
      cnv.arc_to(box.top_right(), box.bottom_right(), corner);
      cnv.arc_to(box.bottom_right(), box.bottom_left(), corner);
      cnv.arc_to(box.bottom_left(), box.top_left(), corner);
      cnv.arc_to(box.top_left(), box.top_right(), corner);
      cnv.close_path();
   }

   void shape_circle(canvas& cnv)
   {
      cnv.add_circle(48, 38, 21);
   }

   void drawn_circle(canvas& cnv)
   {
      cnv.arc({48, 38}, 21, 0, pi);
      cnv.arc({48, 38}, 21, pi, 2 * pi);
      cnv.close_path();
   }

   // Where the quarter turn of view 2 puts each shape, as a shape there.
   constexpr rect turned_box{52, 24, 94, 80};

   void turned_rect(canvas& cnv)
   {
      cnv.add_rect(turned_box);
   }

   void turned_round_rect(canvas& cnv)
   {
      cnv.add_round_rect(turned_box, corner);
   }

   void turned_circle(canvas& cnv)
   {
      cnv.add_circle(72, 52, 21);
   }

   // The views a path is drawn under: as it is, scaled, and turned a
   // quarter. All three keep it and its shadow inside the image.
   void set_view(canvas& cnv, int view)
   {
      if (view == 1)
      {
         cnv.scale(1.25f);
      }
      else if (view == 2)
      {
         cnv.translate(110, 4);
         cnv.rotate(pi / 2);
      }
   }

   // What add puts in the path, filled with paint's fill and a shadow.
   image shadowed(
      draw const& add, draw const& paint
    , float blur, int view, float image_scale)
   {
      image img{120, 100, image_scale};
      render(img, [&](canvas& cnv)
      {
         set_view(cnv, view);
         cnv.shadow_style({6, 8}, blur, rgba(0, 0, 0, 200));
         paint(cnv);
         add(cnv);
         cnv.fill();
      });
      return img;
   }

   void opaque(canvas& cnv)
   {
      cnv.fill_style(red);
   }

   void translucent(canvas& cnv)
   {
      cnv.fill_style(rgba(255, 0, 0, 128));
   }

   void gradient(canvas& cnv)
   {
      canvas::linear_gradient g{box.top_left(), box.bottom_right()};
      g.add_color_stop(0, red);
      g.add_color_stop(1, blue);
      cnv.fill_style(g);
   }

   struct outline
   {
      char const* name;
      draw        shape;
      draw        drawn;
      draw        turned;
   };

   outline const outlines[] =
   {
      {"rectangle", shape_rect, drawn_rect, turned_rect}
    , {"rounded rectangle", shape_round_rect, drawn_round_rect
       , turned_round_rect}
    , {"circle", shape_circle, drawn_circle, turned_circle}
   };
}

TEST_CASE("shadow: a shape's shadow is that of its outline", "[shadow]")
{
   // Sharp, and with a blur of 6 and of 16, as it is and scaled, at both
   // image scales.
   for (auto const& o : outlines)
   {
      for (float blur : {0.0f, 6.0f, 16.0f})
      {
         for (int view : {0, 1})
         {
            for (float scale : {1.0f, 2.0f})
            {
               INFO(o.name << ", blur " << blur << ", view " << view
                  << ", image scale " << scale);
               auto a = shadowed(o.shape, opaque, blur, view, scale);
               auto b = shadowed(o.drawn, opaque, blur, view, scale);
               CHECK(golden_differences(a, b) == 0);
            }
         }
      }
   }
}

TEST_CASE("shadow: a shape turned a quarter", "[shadow]")
{
   // A shape turned a quarter is the same shape lying on its side, and
   // casts the same shadow: the shadow's offset and blur do not turn.
   for (auto const& o : outlines)
   {
      for (float blur : {0.0f, 6.0f, 16.0f})
      {
         for (float scale : {1.0f, 2.0f})
         {
            INFO(o.name << ", blur " << blur << ", image scale " << scale);
            auto a = shadowed(o.shape, opaque, blur, 2, scale);
            auto b = shadowed(o.turned, opaque, blur, 0, scale);
            CHECK(golden_differences(a, b) == 0);
         }
      }
   }
}

TEST_CASE("shadow: whatever fills the shape", "[shadow]")
{
   // A translucent fill casts a fainter shadow that shows through it, and
   // an opaque gradient casts a full one.
   for (auto const& o : outlines)
   {
      for (draw paint : {draw{translucent}, draw{gradient}})
      {
         INFO(o.name);
         auto a = shadowed(o.shape, paint, 6, 0, 1);
         auto b = shadowed(o.drawn, paint, 6, 0, 1);
         CHECK(golden_differences(a, b) == 0);
      }
   }
}

TEST_CASE("shadow: every shape in a path casts one", "[shadow]")
{
   // Two rectangles in one path, sharp and blurred.
   for (float blur : {0.0f, 6.0f})
   {
      image img{100, 100, 1};
      render(img, [blur](canvas& cnv)
      {
         cnv.shadow_style({0, 40}, blur, blue);
         cnv.fill_style(red);
         cnv.add_rect(10, 10, 30, 30);
         cnv.add_rect(60, 10, 30, 30);
         cnv.fill();
      });
      INFO("blur " << blur);
      CHECK(near(pixel_at(img, 25, 65), blue8));
      CHECK(near(pixel_at(img, 75, 65), blue8));
   }

   {
      // A rectangle after other segments.
      image img{100, 100, 1};
      render(img, [](canvas& cnv)
      {
         cnv.shadow_style({0, 45}, 0, blue);
         cnv.fill_style(red);
         cnv.move_to(10, 10);
         cnv.line_to(40, 10);
         cnv.line_to(10, 40);
         cnv.close_path();
         cnv.add_rect(60, 10, 30, 30);
         cnv.fill();
      });
      CHECK(near(pixel_at(img, 18, 63), blue8));
      CHECK(near(pixel_at(img, 75, 70), blue8));
   }
}

TEST_CASE("shadow: a path kept past a paint", "[shadow]")
{
   // fill_preserve and stroke_preserve keep the path. A shape added
   // after either joins what was kept, and both cast a shadow when the
   // path is filled.
   for (bool stroked : {false, true})
   {
      image img{100, 100, 1};
      render(img, [stroked](canvas& cnv)
      {
         cnv.fill_style(red);
         cnv.stroke_style(red);
         cnv.add_rect(10, 10, 30, 30);
         if (stroked)
            cnv.stroke_preserve();
         else
            cnv.fill_preserve();
         cnv.shadow_style({0, 40}, 0, blue);
         cnv.add_rect(60, 10, 30, 30);
         cnv.fill();
      });
      INFO((stroked? "stroke_preserve" : "fill_preserve"));
      CHECK(near(pixel_at(img, 25, 65), blue8));
      CHECK(near(pixel_at(img, 75, 65), blue8));
   }
}

TEST_CASE("shadow: a path set aside keeps its shadow", "[shadow]")
{
   // A one-step rectangle and text are drawn without touching the path
   // being built. The circle in it, and the square added after them, are
   // still filled, each with its shadow; the rectangle has cast its own.
   image img{100, 100, 1};
   render(img, [](canvas& cnv)
   {
      cnv.shadow_style({40, 0}, 0, blue);
      cnv.fill_style(red);
      cnv.add_circle(30, 30, 15);
      cnv.fill_rect(10, 70, 10, 10);
      cnv.stroke_text("x", 10, 95);
      cnv.add_rect(10, 50, 10, 10);
      cnv.fill();
   });
   CHECK(near(pixel_at(img, 30, 30), red8));
   CHECK(near(pixel_at(img, 70, 30), blue8));
   CHECK(near(pixel_at(img, 15, 55), red8));
   CHECK(near(pixel_at(img, 55, 55), blue8));
   CHECK(near(pixel_at(img, 15, 75), red8));
   CHECK(near(pixel_at(img, 55, 75), blue8));
}

#endif
