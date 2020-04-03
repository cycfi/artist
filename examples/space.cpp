/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include "app.hpp"

using namespace cycfi::artist;
auto constexpr window_size = extent{ 640.0f, 480.0f };

auto space = picture{ "space.jpg" };
auto size = space.size();
float x_incr = 0.5;
float y_incr = 0.5;
float x = 0;
float y = 0;

void draw(canvas& cnv)
{
   cnv.translate(x, y);
   cnv.draw(space);
   x += x_incr;
   y += y_incr;

   if (x > 0)
      x_incr = -x_incr;
   if (y > 0)
      y_incr = -y_incr;
   if (x < -(size.x-640))
      x_incr = -x_incr;
   if (y < -(size.y-480))
      y_incr = -y_incr;

   cnv.translate(-x, -y);
   print_elapsed(cnv, window_size);
}

int main(int argc, const char* argv[])
{
   return run_app(argc, argv, window_size, colors::black,true);
}

