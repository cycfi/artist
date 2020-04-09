/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include "app.hpp"

using namespace cycfi::artist;

constexpr auto window_size = extent{ 640, 480 };

void draw(canvas& cnv)
{
   auto stroke_fill =
      [&](path const& p, color fill_c, color stroke_c)
      {
         cnv.path(p);
         cnv.fill_color(fill_c);
         cnv.stroke_color(stroke_c);
         cnv.fill();
         cnv.line_width(5);
         cnv.path(p);
         cnv.stroke();
      };

   auto stroke =
      [&](path const& p, color stroke_c)
      {
         cnv.path(p);
         cnv.stroke_color(stroke_c);
         cnv.line_width(5);
         cnv.stroke();
      };

   {
      auto save = cnv.new_state();
      cnv.scale(0.5, 0.5);
      cnv.translate(-90, 0);

      path p1{ "M300,200 h-150 a150,150 0 1,0 150,-150 z" };
      stroke_fill(p1, colors::red.opacity(0.8), colors::ivory);

      path p2{ "M275,175 v-150 a150,150 0 0,0 -150,150 z" };
      stroke_fill(p2, colors::blue.opacity(0.8), colors::ivory);

      cnv.translate(-120, 0);
      path p3{
         "M600,350 l 50,-25"
         "a25,25 -30 0,1 50,  -25 l 50,-25"
         "a25,50 -30 0,1 50,-25 l 50,-25"
         "a25,75 -30 0,1 50,-25 l 50,-25"
         "a25,100 -30 0,1 50,-25 l 50,-25"
      };
      stroke(p3, colors::ivory);
   }
}

int main(int argc, char const* argv[])
{
   return run_app(argc, argv, window_size, colors::gray[10]);
}

