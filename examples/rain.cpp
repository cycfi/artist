/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include "app.hpp"
#include <cstdlib>

using namespace cycfi::artist;

///////////////////////////////////////////////////////////////////////////////
// Ported from Rainbow Rain animation:
// https://onaircode.com/awesome-html5-canvas-examples-source-code/
///////////////////////////////////////////////////////////////////////////////

#if defined(ARTIST_SKIA)
constexpr auto persistence = 0.15;
#else
constexpr auto persistence = 0.04;
#endif

constexpr auto window_size = extent{ 640, 360 };
constexpr auto w = window_size.x;
constexpr auto h = window_size.y;
constexpr int total = w;
constexpr auto accelleration = 0.05;
constexpr auto repaint_color = rgb(0, 0, 0).opacity(persistence);
constexpr auto portion = 360.0f/total;

float dots[total];
float dots_vel[total];

float random_size()
{
   return float(std::rand()) / (RAND_MAX);
}

void draw(canvas& cnv)
{
   cnv.fill_style(repaint_color);
   cnv.fill_rect({ 0, 0, window_size });
   for (auto i = 0; i < total; ++i)
   {
      auto current_y = dots[i] - 1;
      dots[i] += dots_vel[i] += accelleration;
      cnv.fill_style(hsl(portion * i, 0.8, 0.5));
      cnv.fill_rect({
         float(i)
         , current_y
         , float(i+1)
         , (current_y + dots_vel[i] + 1) * 1.1f
      });

      if (dots[i] > h && random_size() < .01)
         dots[i] = dots_vel[i] = 0;
   }

   print_elapsed(cnv, window_size);
}

void init()
{
   for (auto i = 0; i < total; ++i)
   {
      dots[i] = h;
      dots_vel[i] = 10;
   }
}

int main(int argc, const char* argv[])
{
   init();
   return run_app(argc, argv, window_size, colors::gray[10], true);
}

