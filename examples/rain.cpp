/*=============================================================================
   Copyright (c) 2016-2023 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include "app.hpp"
#include <cstdlib>
#include <vector>

using namespace cycfi::artist;

///////////////////////////////////////////////////////////////////////////////
// Ported from Rainbow Rain animation:
// https://onaircode.com/awesome-html5-canvas-examples-source-code/
///////////////////////////////////////////////////////////////////////////////

#if defined(ARTIST_SKIA) && !defined(linux)
constexpr auto persistence = 0.10;
#else
constexpr auto persistence = 0.04;
#endif

constexpr auto window_size = extent{640, 360};
constexpr auto accelleration = 0.05;
constexpr auto repaint_color = rgb(0, 0, 0);

// One column of rain per device-independent x pixel. These grow as the window
// widens (reflow), so the number of columns tracks cnv.clip_extent().width().
std::vector<float> dots;
std::vector<float> dots_vel;
float opacity = 1.0;

float random_size()
{
   return float(std::rand()) / (RAND_MAX);
}

// Make sure we have state for `total` columns. New columns start at the bottom
// so they fade in like the existing ones.
void ensure_columns(int total, float h)
{
   if (int(dots.size()) < total)
   {
      auto old = dots.size();
      dots.resize(total);
      dots_vel.resize(total);
      for (auto i = old; i < dots.size(); ++i)
      {
         dots[i] = h;
         dots_vel[i] = 10;
      }
   }
}

void rain(canvas& cnv, float w, float h)
{
   int const total = int(w);
   float const portion = 360.0f / (total ? total : 1);
   ensure_columns(total, h);

   cnv.fill_style(repaint_color.opacity(opacity));
   cnv.fill_rect({0, 0, w, h});
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

   if (opacity > persistence)
      opacity *= 0.8;
   print_elapsed(cnv, {w, h}, colors::black.opacity(0.1));
}

void draw(canvas& cnv)
{
   // Reflow to the live window: spawn columns across the full width and keep
   // the offscreen buffer sized to the window.
   auto const bounds = cnv.clip_extent();
   float const w = bounds.width();
   float const h = bounds.height();

   static image offscreen{extent{w, h}};
   static float cached_w = w;
   static float cached_h = h;
   if (w != cached_w || h != cached_h)
   {
      offscreen = image{extent{w, h}};
      cached_w = w;
      cached_h = h;
   }
   {
      auto ctx = offscreen_image{offscreen};
      auto offscreen_cnv = canvas{ctx.context()};
      rain(offscreen_cnv, w, h);
   }
   cnv.draw(offscreen);
}

int main(int argc, char const* argv[])
{
   return run_app(argc, argv, window_size, colors::gray[10], true);
}
