/*=============================================================================
   Copyright (c) 2016-2023 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#if !defined(ARTIST_TEST_APP_MARCH_12_2020)
#define ARTIST_TEST_APP_MARCH_12_2020

#include <infra/support.hpp>
#include <artist/canvas.hpp>
#include <string>
#include <algorithm>
#include <vector>
#include <chrono>
#include <cstdio>

using cycfi::artist::canvas;
using cycfi::artist::extent;
using cycfi::artist::color;
using cycfi::artist::point;
using cycfi::artist::rgba;
using duration = std::chrono::duration<double>;
namespace colors = cycfi::artist::colors;

int            run_app(
                  int argc
                , char const* argv[]
                , extent window_size
                , color background_color = colors::white
                , bool animate = false
               );

void           draw(canvas& cnv);
void           print_elapsed(
                  canvas& cnv
                , point br
                , color bkd = colors::black
                , color c = colors::white
               );

// Scale a fixed `design`-sized drawing to fit the current window, preserving
// aspect ratio and centering (letterbox). Intended for static examples: call
// at the top of draw(). The host save/restores the canvas each frame, so the
// transform is reset automatically. Reflowing examples (e.g. rain, shadow)
// should instead read cnv.clip_extent() directly.
//
// `bkd` fills the WHOLE window first (in window coordinates, before the fit
// transform) so the letterbox margins match the drawing's background instead of
// showing the host's clear color.
// Automated resize benchmark. Sweeps the window through a fixed set of sizes
// (small → large, multiple passes) WITHOUT user interaction, timing each
// resize+redraw via the host-supplied callback, then prints aggregate stats.
// Enabled by the host when ARTIST_RESIZE_BENCH is set. `resize_render(w,h)`
// must size the drawable to w×h physical px, draw, and flush (no vsync wait).
template <typename ResizeRender>
inline void run_resize_bench(char const* label, ResizeRender&& resize_render)
{
   static int const sizes[][2] =
   {
      {200,150}, {320,240}, {480,360}, {640,480}, {800,600},
      {1024,768}, {1280,960}, {1440,1080}, {1600,1200}, {1920,1440}
   };
   constexpr int passes = 6;

   std::vector<double> ms;
   ms.reserve(passes * (sizeof(sizes)/sizeof(sizes[0])));
   for (int p = 0; p < passes; ++p)
      for (auto const& s : sizes)
      {
         auto const t0 = std::chrono::steady_clock::now();
         resize_render(s[0], s[1]);
         auto const t1 = std::chrono::steady_clock::now();
         ms.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
      }

   std::sort(ms.begin(), ms.end());
   auto pct = [&](double q) { return ms[std::min(ms.size()-1, std::size_t(q * ms.size()))]; };
   double sum = 0; for (double v : ms) sum += v;
   std::fprintf(stderr,
      "[bench] %-14s N=%zu  min=%.2f  med=%.2f  p95=%.2f  max=%.2f  mean=%.2f ms\n",
      label, ms.size(), ms.front(), pct(0.5), pct(0.95), ms.back(), sum/ms.size());
}

inline void scale_to_fit(canvas& cnv, extent design, color bkd)
{
   auto const b = cnv.clip_extent();
   cnv.fill_style(bkd);
   cnv.fill_rect(b);
   float const s = std::min(b.width() / design.x, b.height() / design.y);
   cnv.translate({(b.width() - design.x * s) / 2, (b.height() - design.y * s) / 2});
   cnv.scale({s, s});
}

#endif