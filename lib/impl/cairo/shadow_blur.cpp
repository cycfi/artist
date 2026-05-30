/*=============================================================================
   Copyright (c) 2016-2024 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include "shadow_blur.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

// ---------------------------------------------------------------------------
// SIMD capability detection
// ---------------------------------------------------------------------------
#if defined(__aarch64__) || defined(__ARM_NEON__)
#  define ARTIST_NEON 1
#  include <arm_neon.h>
#elif defined(__SSE2__) || defined(_M_X64) || defined(_M_AMD64)
#  define ARTIST_SSE2 1
#  include <emmintrin.h>   // SSE2
#endif

namespace cycfi::artist
{
   // -------------------------------------------------------------------------
   // alpha_extract
   // Pulls the alpha byte (position 3 in BGRA / ARGB32 little-endian) from
   // each pixel into a packed byte array.
   // -------------------------------------------------------------------------
   void alpha_extract(
      const uint8_t* argb32, int stride,
      uint8_t* alpha, int w, int h)
   {
      for (int y = 0; y < h; ++y)
      {
         const uint8_t* src = argb32 + y * stride;
         uint8_t*       dst = alpha  + y * w;
         int x = 0;

#if defined(ARTIST_NEON)
         // vld4q_u8 deinterleaves 4 channels; val[3] is the alpha lane.
         for (; x + 16 <= w; x += 16)
         {
            uint8x16x4_t px = vld4q_u8(src + x * 4);
            vst1q_u8(dst + x, px.val[3]);
         }
#elif defined(ARTIST_SSE2)
         // srli_epi32 by 24 brings byte-3 to byte-0 in each 32-bit lane;
         // two rounds of packs narrow 32-bit lanes down to 8-bit bytes.
         for (; x + 16 <= w; x += 16)
         {
            __m128i p0 = _mm_loadu_si128((const __m128i*)(src + x * 4));
            __m128i p1 = _mm_loadu_si128((const __m128i*)(src + x * 4 + 16));
            __m128i p2 = _mm_loadu_si128((const __m128i*)(src + x * 4 + 32));
            __m128i p3 = _mm_loadu_si128((const __m128i*)(src + x * 4 + 48));
            p0 = _mm_srli_epi32(p0, 24);
            p1 = _mm_srli_epi32(p1, 24);
            p2 = _mm_srli_epi32(p2, 24);
            p3 = _mm_srli_epi32(p3, 24);
            __m128i q0 = _mm_packs_epi32(p0, p1);
            __m128i q1 = _mm_packs_epi32(p2, p3);
            _mm_storeu_si128((__m128i*)(dst + x), _mm_packus_epi16(q0, q1));
         }
#endif
         // Scalar tail (also the full loop when no SIMD is available).
         for (; x < w; ++x)
            dst[x] = src[x * 4 + 3];
      }
   }

   // -------------------------------------------------------------------------
   // alpha_writeback
   // Writes the packed alpha bytes back into byte-3 of each BGRA pixel, leaving
   // the R, G, B channels untouched.
   // -------------------------------------------------------------------------
   void alpha_writeback(
      uint8_t* argb32, int stride,
      const uint8_t* alpha, int w, int h)
   {
      for (int y = 0; y < h; ++y)
      {
         uint8_t*       dst = argb32 + y * stride;
         const uint8_t* src = alpha  + y * w;
         int x = 0;

#if defined(ARTIST_NEON)
         // vld4q_u8 / vst4q_u8 interleave/deinterleave all 4 channels cleanly.
         for (; x + 16 <= w; x += 16)
         {
            uint8x16x4_t px = vld4q_u8(dst + x * 4);
            px.val[3] = vld1q_u8(src + x);
            vst4q_u8(dst + x * 4, px);
         }
#elif defined(ARTIST_SSE2)
         // Unpack 4 alpha bytes to 32-bit, shift to the high byte, then OR
         // into the pixel after masking out the old alpha.
         __m128i zero = _mm_setzero_si128();
         __m128i mask = _mm_set1_epi32(0x00FFFFFF);
         for (; x + 4 <= w; x += 4)
         {
            int a_int;
            std::memcpy(&a_int, src + x, 4);
            __m128i a4 = _mm_cvtsi32_si128(a_int);
            a4 = _mm_unpacklo_epi8(a4, zero);   // 4 × uint16 in low 64 bits
            a4 = _mm_unpacklo_epi16(a4, zero);  // 4 × uint32
            a4 = _mm_slli_epi32(a4, 24);        // move to alpha byte position
            __m128i px = _mm_loadu_si128((const __m128i*)(dst + x * 4));
            px = _mm_or_si128(_mm_and_si128(px, mask), a4);
            _mm_storeu_si128((__m128i*)(dst + x * 4), px);
         }
#endif
         for (; x < w; ++x)
            dst[x * 4 + 3] = src[x];
      }
   }

   // -------------------------------------------------------------------------
   // shadow_reconstruct
   // Writes premultiplied ARGB32: each pixel becomes (sb*a/255, sg*a/255,
   // sr*a/255, a). Correct for any shadow color — blurring only operates on
   // the alpha channel, so RGB must be reconstructed from the shadow color
   // rather than left at their pre-blur values (which are 0 outside glyphs).
   // -------------------------------------------------------------------------
   void shadow_reconstruct(
      uint8_t* argb32, int stride,
      const uint8_t* alpha, int w, int h,
      uint8_t sr, uint8_t sg, uint8_t sb)
   {
      for (int y = 0; y < h; ++y)
      {
         uint8_t*       dst = argb32 + y * stride;
         const uint8_t* src = alpha  + y * w;
         int x = 0;

#if defined(ARTIST_NEON)
         uint8x8_t vsr = vdup_n_u8(sr);
         uint8x8_t vsg = vdup_n_u8(sg);
         uint8x8_t vsb = vdup_n_u8(sb);
         for (; x + 8 <= w; x += 8)
         {
            uint8x8_t a   = vld1_u8(src + x);
            uint8x8_t r8  = vrshrn_n_u16(vmull_u8(a, vsr), 8);
            uint8x8_t g8  = vrshrn_n_u16(vmull_u8(a, vsg), 8);
            uint8x8_t b8  = vrshrn_n_u16(vmull_u8(a, vsb), 8);
            uint8x8x4_t bgra = {{ b8, g8, r8, a }};
            vst4_u8(dst + x * 4, bgra);
         }
#elif defined(ARTIST_SSE2)
         __m128i zero  = _mm_setzero_si128();
         __m128i vsr16 = _mm_set1_epi16(static_cast<short>(sr));
         __m128i vsg16 = _mm_set1_epi16(static_cast<short>(sg));
         __m128i vsb16 = _mm_set1_epi16(static_cast<short>(sb));
         for (; x + 8 <= w; x += 8)
         {
            // Load 8 alpha bytes, widen to u16 for multiply.
            __m128i a8  = _mm_loadl_epi64((const __m128i*)(src + x));
            __m128i a16 = _mm_unpacklo_epi8(a8, zero);
            // Compute premultiplied channels: (channel * alpha) >> 8.
            __m128i r8  = _mm_packus_epi16(_mm_srli_epi16(_mm_mullo_epi16(a16, vsr16), 8), zero);
            __m128i g8  = _mm_packus_epi16(_mm_srli_epi16(_mm_mullo_epi16(a16, vsg16), 8), zero);
            __m128i b8  = _mm_packus_epi16(_mm_srli_epi16(_mm_mullo_epi16(a16, vsb16), 8), zero);
            // Interleave 4 channels into BGRA byte order (Cairo ARGB32 on LE).
            __m128i bg      = _mm_unpacklo_epi8(b8, g8);   // [b0,g0, b1,g1, ...]
            __m128i ra      = _mm_unpacklo_epi8(r8, a8);   // [r0,a0, r1,a1, ...]
            __m128i bgra_lo = _mm_unpacklo_epi16(bg, ra);  // pixels 0-3
            __m128i bgra_hi = _mm_unpackhi_epi16(bg, ra);  // pixels 4-7
            _mm_storeu_si128((__m128i*)(dst + x * 4),      bgra_lo);
            _mm_storeu_si128((__m128i*)(dst + x * 4 + 16), bgra_hi);
         }
#endif
         for (; x < w; ++x)
         {
            uint32_t a        = src[x];
            dst[x * 4 + 0]    = static_cast<uint8_t>((sb * a + 127) >> 8);
            dst[x * 4 + 1]    = static_cast<uint8_t>((sg * a + 127) >> 8);
            dst[x * 4 + 2]    = static_cast<uint8_t>((sr * a + 127) >> 8);
            dst[x * 4 + 3]    = static_cast<uint8_t>(a);
         }
      }
   }

   // -------------------------------------------------------------------------
   // box_blur_h_1ch  (internal)
   // Single horizontal box-blur pass on a packed single-channel buffer.
   // Called blur_passes times per axis by approx_gaussian_blur_1ch to accumulate
   // the Gaussian-approximating kernel. Uses prefix sums so the interior
   // output loop is branch-free and SIMD-friendly.
   // cum must be at least w+1 elements.
   // -------------------------------------------------------------------------
   static void box_blur_h_1ch(
      const uint8_t* src, uint8_t* dst,
      int w, int h, int radius, int32_t* cum)
   {
      float inv = 1.0f / static_cast<float>(2 * radius + 1);

      for (int y = 0; y < h; ++y)
      {
         const uint8_t* row = src + y * w;
         uint8_t*       out = dst + y * w;

         // Build prefix sum (serial O(w), unavoidable).
         cum[0] = 0;
         for (int x = 0; x < w; ++x)
            cum[x + 1] = cum[x] + row[x];

         // Left border: window [0, x+radius] — shrinks at the edge.
         int x_mid_end = w - radius;
         for (int x = 0; x < std::min(radius, w); ++x)
         {
            int hi = std::min(x + radius + 1, w);
            out[x] = static_cast<uint8_t>(cum[hi] / hi);
         }

         // Interior: fixed window [x-radius, x+radius+1], parallelisable.
         int x = radius;

#if defined(ARTIST_NEON)
         {
            float32x4_t inv4 = vdupq_n_f32(inv);
            for (; x + 4 <= x_mid_end; x += 4)
            {
               int32x4_t hi_i = vld1q_s32(cum + x + radius + 1);
               int32x4_t lo_i = vld1q_s32(cum + x - radius);
               int32x4_t diff = vsubq_s32(hi_i, lo_i);
               int32x4_t ri   = vcvtq_s32_f32(vmulq_f32(vcvtq_f32_s32(diff), inv4));
               uint8x8_t r8 = vqmovn_u16(vcombine_u16(vqmovun_s32(ri), vdup_n_u16(0)));
               vst1_lane_u32((uint32_t*)(out + x), vreinterpret_u32_u8(r8), 0);
            }
         }
#elif defined(ARTIST_SSE2)
         {
            __m128 inv4 = _mm_set1_ps(inv);
            for (; x + 4 <= x_mid_end; x += 4)
            {
               __m128i hi_i = _mm_loadu_si128((const __m128i*)(cum + x + radius + 1));
               __m128i lo_i = _mm_loadu_si128((const __m128i*)(cum + x - radius));
               __m128i diff = _mm_sub_epi32(hi_i, lo_i);
               __m128i ri   = _mm_cvttps_epi32(_mm_mul_ps(_mm_cvtepi32_ps(diff), inv4));
               ri = _mm_packs_epi32(ri, ri);
               ri = _mm_packus_epi16(ri, ri);
               int tmp32 = _mm_cvtsi128_si32(ri);
               std::memcpy(out + x, &tmp32, 4);
            }
         }
#endif
         for (; x < x_mid_end; ++x)
            out[x] = static_cast<uint8_t>((cum[x + radius + 1] - cum[x - radius]) * inv);

         // Right border: window [x-radius, w] — shrinks at the edge.
         for (int bx = std::max(radius, x_mid_end); bx < w; ++bx)
         {
            int lo  = bx - radius;
            int cnt = w - lo;
            out[bx] = static_cast<uint8_t>((cum[w] - cum[lo]) / cnt);
         }
      }
   }

   // -------------------------------------------------------------------------
   // transpose_1ch  (internal)
   // Transposes a w×h single-channel image to h×w using 16×16 cache-blocking.
   // -------------------------------------------------------------------------
   static void transpose_1ch(
      const uint8_t* src, uint8_t* dst, int w, int h)
   {
      constexpr int B = 16;
      for (int y = 0; y < h; y += B)
      {
         int y_end = std::min(y + B, h);
         for (int x = 0; x < w; x += B)
         {
            int x_end = std::min(x + B, w);
            for (int i = y; i < y_end; ++i)
               for (int j = x; j < x_end; ++j)
                  dst[j * h + i] = src[i * w + j];
         }
      }
   }

   // -------------------------------------------------------------------------
   // approx_gaussian_blur_1ch
   // -------------------------------------------------------------------------
   void approx_gaussian_blur_1ch(
      uint8_t* alpha, uint8_t* tmp,
      int32_t* cum, int w, int h, float sigma)
   {
      int r = blur_box_radius(sigma);
      for (int pass = 0; pass < blur_passes; ++pass)
      {
         // Horizontal pass: alpha (w×h) → tmp
         box_blur_h_1ch(alpha, tmp, w, h, r, cum);
         // Transpose into alpha (now logically h×w)
         transpose_1ch(tmp, alpha, w, h);
         // Vertical pass as horizontal on transposed image: alpha (h×w) → tmp
         box_blur_h_1ch(alpha, tmp, h, w, r, cum);
         // Transpose back to w×h
         transpose_1ch(tmp, alpha, h, w);
      }
   }
}
