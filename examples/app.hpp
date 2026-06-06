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