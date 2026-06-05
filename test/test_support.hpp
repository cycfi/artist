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

// Defined in golden.cpp.
void background(canvas& cnv);
void compare_golden(image const& pm, std::string name);

// Like compare_golden, but bootstraps: if the golden does not exist yet, save
// the current render as the golden (first run) instead of failing; on later
// runs, compare against it. Backend/platform-specific, like all goldens.
void snapshot_golden(image const& pm, std::string name);

#endif
