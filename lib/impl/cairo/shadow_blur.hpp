/*=============================================================================
   Copyright (c) 2016-2024 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]

   Cairo shadow blur — SIMD-accelerated alpha-channel Gaussian-approximating
   box blur utilities. Include only from Cairo backend implementation files.
=============================================================================*/
#pragma once
#include <cstdint>
#include <cmath>
#include <algorithm>

namespace cycfi::artist
{
   // Number of box-blur passes used to approximate a Gaussian. The combined
   // support of N passes is ±N·radius, and the support-to-sigma ratio is
   // sqrt(3N). With 4 passes the hard cutoff lands at ~3.46σ (Gaussian ≈ 0.27%),
   // so the box blur's finite square support is not visible as a clipped edge —
   // even on bright glows against dark backgrounds. (3 passes cuts off at ~2.8σ
   // ≈ 1.8%, which shows as flat top/left/bottom/right edges.)
   constexpr int blur_passes = 4;

   // Box-blur radius (half-width) that, repeated blur_passes times, approximates
   // a Gaussian of the given sigma. Solves N·(r²+r)/3 = σ² for r.
   inline int blur_box_radius(float sigma)
   {
      double n = blur_passes;
      double r = (-1.0 + std::sqrt(1.0 + 12.0 * sigma * sigma / n)) * 0.5;
      return std::max(1, static_cast<int>(std::lround(r)));
   }

   // Surface margin (in pixels) needed around a shape so the blur's full support
   // is never clipped by the surface boundary: blur_passes · radius, plus a pad.
   inline int blur_margin(float sigma)
   {
      return blur_passes * blur_box_radius(sigma) + 2;
   }

   // Extract the alpha channel (byte 3 of each BGRA pixel in ARGB32 format)
   // from an image surface into a contiguous single-channel buffer.
   void alpha_extract(
      const uint8_t* argb32, int stride,
      uint8_t* alpha, int w, int h);

   // Write a contiguous single-channel alpha buffer back into an ARGB32
   // surface, leaving the R, G, B channels unchanged.
   void alpha_writeback(
      uint8_t* argb32, int stride,
      const uint8_t* alpha, int w, int h);

   // Reconstruct premultiplied ARGB32 pixels from a blurred alpha buffer and
   // a fixed shadow color (sr, sg, sb in 0–255). Each output pixel becomes
   // (sb*a/255, sg*a/255, sr*a/255, a) — the correct premultiplied form for
   // any shadow color, not just black.
   void shadow_reconstruct(
      uint8_t* argb32, int stride,
      const uint8_t* alpha, int w, int h,
      uint8_t sr, uint8_t sg, uint8_t sb);

   // Gaussian-approximating box blur on a single-channel (alpha) buffer
   // in-place. Repeats a horizontal box-blur pass blur_passes times (with
   // a transpose trick for the vertical direction). By the Central Limit
   // Theorem, N repeated box blurs converge to a Gaussian-shaped kernel;
   // this is not a true Gaussian (the kernel has finite support and a
   // B-spline profile), but is indistinguishable at typical shadow radii.
   // cum must point to at least (max(w, h) + 1) int32_t elements.
   void approx_gaussian_blur_1ch(
      uint8_t* alpha, uint8_t* tmp,
      int32_t* cum, int w, int h, float sigma);
}
