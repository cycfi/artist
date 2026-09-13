/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]

   Shared support for the artist test suite: common includes, the offscreen
   render constants, and the golden-image comparison entry point. Each themed
   test translation unit includes this; exactly one TU (test_main.cpp) defines
   the Catch main.
=============================================================================*/
#if !defined(ARTIST_TEST_SUPPORT_JUNE_5_2026)
#define ARTIST_TEST_SUPPORT_JUNE_5_2026

#if defined(_WIN32)
# ifndef UNICODE
#  define UNICODE
# endif
#endif

#include <infra/catch.hpp>
#include <artist/affine_transform.hpp>
#include "app_paths.hpp"
#include <cmath>
#include <cstdint>
#include <infra/support.hpp>
#include <memory>
#include <algorithm>
#include <iomanip>
#include <vector>

using namespace cycfi::artist;
using namespace font_constants;
using cycfi::codepoint;

auto constexpr window_size = point{640.0f, 480.0f};
auto constexpr bkd_color = rgba(54, 52, 55, 255);

// Golden scenes render at this scale on every backend, so comparisons are at
// one resolution instead of Quartz's implicit display backing scale. The
// golden image (a loaded PNG) is window_size * golden_scale pixels.
auto constexpr golden_scale = 2.0f;

// Defined in golden.cpp.
void background(canvas& cnv);
void compare_golden(image const& pm, std::string name);

// Like compare_golden, but bootstraps: if the golden does not exist yet, save
// the current render as the golden (first run) instead of failing; on later
// runs, compare against it. Backend/platform-specific, like all goldens.
void snapshot_golden(image const& pm, std::string name);

#if defined(ARTIST_RECORDING)
# include <artist/recording.hpp>

// The recording backend keeps a journal instead of pixels. Where the drawing
// backends probe pixels, tests read the journal through these.
inline recording::journal const& recorded(image const& img)
{
   return recording::journal_of(img);
}

inline recording::command const& recorded(image const& img, std::size_t i)
{
   return recording::journal_of(img).commands().at(i);
}

inline bool same_color(color a, color b)
{
   auto eq = [](float x, float y) { return std::abs(x - y) < 0.01f; };
   return eq(a.red, b.red) && eq(a.green, b.green)
      && eq(a.blue, b.blue) && eq(a.alpha, b.alpha);
}

inline bool same_rect(rect a, rect b)
{
   auto eq = [](float x, float y) { return std::abs(x - y) < 0.01f; };
   return eq(a.left, b.left) && eq(a.top, b.top)
      && eq(a.right, b.right) && eq(a.bottom, b.bottom);
}
#endif

#endif
