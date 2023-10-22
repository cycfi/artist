/*=============================================================================
   Copyright (c) 2016-2023 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include "app.hpp"

using namespace cycfi::artist;

constexpr auto window_size = extent{640, 420};

void draw(canvas& cnv)
{
   auto stroke_fill =
      [&](path const& p, color fill_c, color stroke_c)
      {
         cnv.add_path(p);
         cnv.fill_color(fill_c);
         cnv.stroke_color(stroke_c);
         cnv.fill();
         cnv.line_width(5);
         cnv.add_path(p);
         cnv.stroke();
      };

   auto stroke =
      [&](path const& p, color stroke_c)
      {
         cnv.add_path(p);
         cnv.stroke_color(stroke_c);
         cnv.line_width(5);
         cnv.stroke();
      };

   auto dot =
      [&](float x, float y)
      {
         cnv.add_circle(x, y, 10);
         cnv.fill_color(colors::white.opacity(0.5));
         cnv.fill();
      };

   // These paths are defined using SVG-style strings lifted off the
   // W3C SVG documentation: https://www.w3.org/TR/SVG/paths.html

   {
      auto save = cnv.new_state();
      cnv.scale(0.5, 0.5);

      cnv.translate(-40, 0);
      path p1{"M 100 100 L 300 100 L 200 300 z"};
      stroke_fill(p1, colors::green.opacity(0.5), colors::ivory);

      cnv.translate(220, 0);
      path p2{"M100,200 C100,100 250,100 250,200 S400,300 400,200"};
      stroke(p2, colors::light_sky_blue);

      cnv.translate(-150, 250);
      path p3{"M200,300 Q400,50 600,300 T1000,300"};
      stroke(p3, colors::light_sky_blue);

      path p4{"M200,300 L400,50 L600,300 L800,550 L1000,300"};
      stroke(p4, colors::light_gray.opacity(0.5));
      dot(200, 300);
      dot(600, 300);
      dot(1000, 300);
      dot(400, 50);
      dot(800, 550);
      cnv.translate(150, -250);

      cnv.translate(350, 0);
      path p5{"M300,200 h-150 a150,150 0 1,0 150,-150 z"};
      stroke_fill(p5, colors::red.opacity(0.8), colors::ivory);

      path p6{"M275,175 v-150 a150,150 0 0,0 -150,150 z"};
      stroke_fill(p6, colors::blue.opacity(0.8), colors::ivory);

      cnv.translate(-350, 200);
      path p7{
         "M600,350 l 50,-25"
         "a25,25 -30 0,1 50,-25 l 50,-25"
         "a25,50 -30 0,1 50,-25 l 50,-25"
         "a25,75 -30 0,1 50,-25 l 50,-25"
         "a25,100 -30 0,1 50,-25 l 50,-25"
      };
      stroke(p7, colors::ivory);
   }

   cnv.font(font_descr{"Open Sans", 12}.thin());
   cnv.fill_color(colors::antique_white);
   cnv.text_align(cnv.bottom);
   auto url = "https://www.w3.org/TR/SVG/paths.html";
   cnv.fill_text(url, {5, window_size.y-5});
}

int main(int argc, char const* argv[])
{
   return run_app(argc, argv, window_size, colors::gray[10]);
}

