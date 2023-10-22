/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include "app.hpp"

using namespace cycfi::artist;
auto constexpr window_size = extent{640.0f, 480.0f};

float x_incr = 0.5;
float y_incr = 0.5;
float x = 100;
float y = 100;

void draw(canvas& cnv)
{
   cnv.save();
   cnv.fill_style(colors::white);
   cnv.add_rect({0, 0, window_size});
   cnv.fill();

   cnv.fill_style(colors::black);
   cnv.shadow_style({4, 4}, 6, colors::gray[30]);
   cnv.add_round_rect({10, 10, x, y}, 10);
   cnv.fill();

   x += x_incr;
   y += y_incr;

   if (x > 600 || x < 100)
      x_incr = -x_incr;
   if (y > 440 || y < 100)
      y_incr = -y_incr;

   cnv.restore();
   print_elapsed(cnv, window_size, colors::white, colors::black);
}

int main(int argc, char const* argv[])
{
   return run_app(argc, argv, window_size, colors::white, true);
}

