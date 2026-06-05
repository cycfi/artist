/*=============================================================================
   Copyright (c) 2016-2023 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#if defined(_WIN32)
# ifndef UNICODE
#  define UNICODE
# endif
#endif

#define CATCH_CONFIG_MAIN
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

#if defined(ARTIST_QUARTZ_2D)
   // Initialize the fonts and resources
   struct resource_setter
   {
      resource_setter();
   };
   static resource_setter init_resources;
#endif

void background(canvas& cnv)
{
   cnv.add_rect({{0, 0}, window_size});
   cnv.fill_style(bkd_color);
   cnv.fill();
}

void balloon(canvas& cnv)
{
   // quadratic_curve_to
   cnv.begin_path();
   cnv.line_join(cnv.round_join);
   cnv.move_to(75, 25);
   cnv.quadratic_curve_to({25, 25}, {25, 62.5});
   cnv.quadratic_curve_to({25, 100}, {50, 100});
   cnv.quadratic_curve_to({50, 120}, {30, 125});
   cnv.quadratic_curve_to({60, 120}, {65, 100});
   cnv.quadratic_curve_to({125, 100}, {125, 62.5});
   cnv.quadratic_curve_to({125, 25}, {75, 25});
}

void heart(canvas& cnv)
{
   // bezier_curve_to
   constexpr float y0 = 40, y1 = 50, y2 = 23, y3 = 90, y4 = 45, y5 = 75, y6 = 125;
   cnv.begin_path();
   cnv.move_to({75, y0});
   cnv.bezier_curve_to({75, y4}, {70, y2}, {50, y2});
   cnv.bezier_curve_to({20, y2}, {20, y1}, {20, y1});
   cnv.bezier_curve_to({20, y5}, {y0, y3}, {75, y6});
   cnv.bezier_curve_to({110, y3}, {130, y5}, {130, y1});
   cnv.bezier_curve_to({ 130, y1}, {130, y2}, {100, y2});
   cnv.bezier_curve_to({85, y2}, {75, y4}, {75, y0});
   cnv.fill_style(color{0.2, 0, 0}.opacity(0.4));
}

void basics(canvas& cnv)
{
   auto state = cnv.new_state();

   cnv.add_rect(20, 20, 80, 40);
   cnv.fill_style(colors::navy_blue);
   cnv.fill_preserve();

   cnv.stroke_style(colors::antique_white.opacity(0.8));
   cnv.line_width(0.5);
   cnv.stroke();

   cnv.add_round_rect(40, 35, 80, 45, 8);
   cnv.fill_style(colors::light_sea_green.opacity(0.5));
   cnv.fill();

   cnv.move_to(10, 10);
   cnv.line_to(100, 100);
   cnv.line_width(10);
   cnv.stroke_style(colors::hot_pink);
   cnv.stroke();

   cnv.add_circle(120, 80, 40);
   cnv.line_width(4);
   cnv.fill_style(colors::gold.opacity(0.5));
   cnv.stroke_style(colors::gold);
   cnv.fill_preserve();
   cnv.stroke();

   cnv.translate(120, 0);
   balloon(cnv);
   cnv.stroke_style(colors::light_gray);
   cnv.stroke();

   cnv.translate(-100, 100);
   heart(cnv);
   cnv.line_width(2);
   cnv.stroke_style(color{0.8, 0, 0});
   cnv.stroke_preserve();
   cnv.fill();
}

void transformed(canvas& cnv)
{
   auto state = cnv.new_state();
   cnv.scale(1.5, 1.5);
   cnv.translate(150, 80);
   cnv.rotate(0.8);
   basics(cnv);
}

namespace detail
{
   struct corner_radii
   {
      float top_left, top_right, bottom_right, bottom_left;
      corner_radii operator+(float v) const;
      corner_radii operator-(float v) const;
   };
}

void draw_round_rect(canvas& cnv, rect bounds, detail::corner_radii radius)
{
   constexpr auto pi = 3.14159265358979323846;
   auto l = bounds.left;
   auto t = bounds.top;
   auto r = bounds.right;
   auto b = bounds.bottom;
   radius.top_left =     std::min(radius.top_left,     std::min(bounds.width(), bounds.height()) / 2);
   radius.top_right =    std::min(radius.top_right,    std::min(bounds.width(), bounds.height()) / 2);
   radius.bottom_right = std::min(radius.bottom_right, std::min(bounds.width(), bounds.height()) / 2);
   radius.bottom_left =  std::min(radius.bottom_left,  std::min(bounds.width(), bounds.height()) / 2);

   cnv.begin_path();
   cnv.arc({r-radius.bottom_right, b-radius.bottom_right}, radius.bottom_right, 0,       pi*0.5);
   cnv.arc({l+radius.bottom_left,  b-radius.bottom_left }, radius.bottom_left,  pi*0.5, pi    );
   cnv.arc({l+radius.top_left,     t+radius.top_left    }, radius.top_left,     pi,     pi*1.5);
   cnv.arc({r-radius.top_right,    t+radius.top_right   }, radius.top_right,    pi*1.5, 0      );
   cnv.close_path();
}

void basics2(canvas& cnv)
{
   auto state = cnv.new_state();
   rect bounds = {20, 20, 220, 120};

   {
      draw_round_rect(cnv, bounds, {-1, 40, 40, -1}); // Negative radius

      cnv.fill_style(colors::light_sea_green.opacity(0.8));
      cnv.fill_preserve();

      cnv.stroke_style(colors::antique_white.opacity(0.8));
      cnv.line_width(2);
      cnv.stroke();
   }
   {
      draw_round_rect(cnv, bounds.move(20, 20), {15, 40, 40, 15});

      cnv.fill_style(colors::navy_blue.opacity(0.5));
      cnv.fill_preserve();

      cnv.stroke_style(colors::antique_white.opacity(0.5));
      cnv.line_width(2);
      cnv.stroke();
   }
   {
      draw_round_rect(cnv, bounds.move(40, 40), {0, 0, 0, 0});

      cnv.fill_style(colors::indian_red.opacity(0.5));
      cnv.fill_preserve();

      cnv.stroke_style(colors::antique_white.opacity(0.5));
      cnv.line_width(2);
      cnv.stroke();
   }
}

void rainbow(canvas::gradient& gr)
{
   gr.add_color_stop(0.0/6, colors::red);
   gr.add_color_stop(1.0/6, colors::orange);
   gr.add_color_stop(2.0/6, colors::yellow);
   gr.add_color_stop(3.0/6, colors::green);
   gr.add_color_stop(4.0/6, colors::blue);
   gr.add_color_stop(5.0/6, rgb(0x4B, 0x00, 0x82));
   gr.add_color_stop(6.0/6, colors::violet);
}

void linear_gradient(canvas& cnv)
{
   auto x = 300.0f;
   auto y = 20.0f;
   auto gr = canvas::linear_gradient{x, y, x+300, y};
   rainbow(gr);

   cnv.add_round_rect(x, y, 300, 80, 5);
   cnv.fill_style(gr);
   cnv.fill();
}

void radial_gradient(canvas& cnv)
{
   auto center = point{475, 90};
   auto radius = 75.0f;
   auto gr = canvas::radial_gradient{center, 5, center.move(15, 10), radius};
   gr.add_color_stop(0.0, colors::red);
   gr.add_color_stop(1.0, colors::black);

   cnv.add_circle({center.move(15, 10), radius - 10});
   cnv.fill_style(gr);
   cnv.fill();
}

void stroke_gradient(canvas& cnv)
{
   auto x = 300.0f;
   auto y = 20.0f;
   auto gr = canvas::linear_gradient{x, y, x+300, y+80};
   gr.add_color_stop(0.0, colors::navy_blue);
   gr.add_color_stop(1.0, colors::maroon);

   cnv.add_round_rect(x, y, 300, 80, 5);
   cnv.line_width(8);
   cnv.stroke_style(gr);
   cnv.stroke();
}

void draw_pixmap(canvas& cnv)
{
   image pm{get_images_path() + "logo.png"};
   auto x = 250.0f, y = 120.0f;
   cnv.draw(pm, x, y, 0.4);
}

void line_styles(canvas& cnv)
{
   auto where = point{500, 200};
   cnv.shadow_style({5.0, 5.0}, 5, colors::black);

   cnv.stroke_style(colors::gold);
   cnv.begin_path();
   cnv.line_width(10);
   cnv.line_cap(cnv.butt);
   cnv.move_to(where.x, where.y);
   cnv.line_to(where.x+100, where.y);
   cnv.stroke();

   cnv.stroke_style(colors::sky_blue);
   cnv.begin_path();
   cnv.line_cap(cnv.round);
   cnv.move_to(where.x, where.y+20);
   cnv.line_to(where.x+100, where.y+20);
   cnv.stroke();

   cnv.stroke_style(colors::light_sea_green);
   cnv.begin_path();
   cnv.line_cap(cnv.square);
   cnv.move_to(where.x, where.y+40);
   cnv.line_to(where.x+100, where.y+40);
   cnv.stroke();

   where.x -= 20;

   cnv.stroke_style(colors::gold);
   cnv.begin_path();
   cnv.line_join(cnv.bevel_join);
   cnv.move_to(where.x, where.y+100);
   cnv.line_to(where.x+60, where.y+140);
   cnv.line_to(where.x, where.y+180);
   cnv.stroke();

   cnv.stroke_style(colors::sky_blue);
   cnv.begin_path();
   cnv.line_join(cnv.round_join);
   cnv.move_to(where.x+40, where.y+100);
   cnv.line_to(where.x+100, where.y+140);
   cnv.line_to(where.x+40, where.y+180);
   cnv.stroke();

   cnv.stroke_style(colors::light_sea_green);
   cnv.begin_path();
   cnv.line_join(cnv.miter_join);
   cnv.move_to(where.x+80, where.y+100);
   cnv.line_to(where.x+140, where.y+140);
   cnv.line_to(where.x+80, where.y+180);
   cnv.stroke();
}

void test_draw(canvas& cnv)
{
   background(cnv);
   basics(cnv);
   transformed(cnv);
   linear_gradient(cnv);
   radial_gradient(cnv);
   stroke_gradient(cnv);
   draw_pixmap(cnv);
   line_styles(cnv);
}

void test_draw2(canvas& cnv)
{
   background(cnv);
   basics2(cnv);
}

// ---------------------------------------------------------------------------
// Perceptual image comparison via CIELAB ΔE₀₀ (CIEDE2000)
//
// Cairo pixels() returns premultiplied ARGB32: uint32 = (A<<24)|(R<<16)|(G<<8)|B.
// We composite over black before conversion (premultiplied RGB over black is
// just the premultiplied values themselves), giving us the visible pixel colour.
// ---------------------------------------------------------------------------

// sRGB gamma-encoded byte → linear light [0,1]
static float srgb_to_linear(uint8_t v)
{
   float c = v / 255.0f;
   return c <= 0.04045f ? c / 12.92f
                        : std::pow((c + 0.055f) / 1.055f, 2.4f);
}

// Premultiplied ARGB32 pixel → CIELAB (L*, a*, b*)
static void pixel_to_lab(uint32_t px, float& L, float& a, float& b)
{
   // Unpack — Cairo ARGB32: A=bits31-24, R=bits23-16, G=bits15-8, B=bits7-0
   uint8_t A = (px >> 24) & 0xFF;
   uint8_t R = (px >> 16) & 0xFF;
   uint8_t G = (px >>  8) & 0xFF;
   uint8_t B =  px        & 0xFF;
   (void)A; // composited over black: premultiplied RGB = visible result

   float r = srgb_to_linear(R);
   float g = srgb_to_linear(G);
   float bv = srgb_to_linear(B);

   // Linear sRGB → CIE XYZ D65 (IEC 61966-2-1 matrix)
   float X = 0.4124564f * r + 0.3575761f * g + 0.1804375f * bv;
   float Y = 0.2126729f * r + 0.7151522f * g + 0.0721750f * bv;
   float Z = 0.0193339f * r + 0.1191920f * g + 0.9503041f * bv;

   // XYZ → CIELAB (D65 reference white)
   auto f = [](float t) -> float {
      return t > 0.008856f ? std::cbrt(t) : 7.787f * t + 16.0f / 116.0f;
   };
   float fx = f(X / 0.95047f);
   float fy = f(Y / 1.00000f);
   float fz = f(Z / 1.08883f);

   L = 116.0f * fy - 16.0f;
   a = 500.0f * (fx - fy);
   b = 200.0f * (fy - fz);
}

// CIEDE2000 perceptual colour difference between two CIELAB values.
// Returns ΔE₀₀ ∈ [0, ∞): < 1 imperceptible, 1–2 just noticeable, > 3 clearly visible.
static float delta_e_00(float L1, float a1, float b1,
                        float L2, float a2, float b2)
{
   using std::sqrt; using std::pow; using std::atan2;
   using std::cos;  using std::sin; using std::exp; using std::abs;
   constexpr float pi = 3.14159265358979f;
   auto to_rad = [&](float d) { return d * pi / 180.0f; };
   auto to_deg = [&](float r) { return r * 180.0f / pi; };

   float C1 = sqrt(a1*a1 + b1*b1);
   float C2 = sqrt(a2*a2 + b2*b2);
   float Cavg = (C1 + C2) * 0.5f;
   float Cavg7 = pow(Cavg, 7.0f);
   float G = 0.5f * (1.0f - sqrt(Cavg7 / (Cavg7 + 6103515625.0f))); // 25^7

   float a1p = a1 * (1.0f + G),  a2p = a2 * (1.0f + G);
   float C1p = sqrt(a1p*a1p + b1*b1);
   float C2p = sqrt(a2p*a2p + b2*b2);

   auto hprime = [&](float bp, float ap) -> float {
      if (bp == 0.0f && ap == 0.0f) return 0.0f;
      float h = to_deg(atan2(bp, ap));
      return h < 0.0f ? h + 360.0f : h;
   };
   float h1p = hprime(b1, a1p);
   float h2p = hprime(b2, a2p);

   float dLp = L2 - L1;
   float dCp = C2p - C1p;

   float dhp;
   if (C1p * C2p == 0.0f)         dhp = 0.0f;
   else if (abs(h2p-h1p) <= 180.0f) dhp = h2p - h1p;
   else if (h2p - h1p  >  180.0f) dhp = h2p - h1p - 360.0f;
   else                             dhp = h2p - h1p + 360.0f;

   float dHp = 2.0f * sqrt(C1p * C2p) * sin(to_rad(dhp * 0.5f));

   float Lbarp  = (L1  + L2 ) * 0.5f;
   float Cbarp  = (C1p + C2p) * 0.5f;

   float hbarp;
   if (C1p * C2p == 0.0f)           hbarp = h1p + h2p;
   else if (abs(h1p-h2p) <= 180.0f) hbarp = (h1p + h2p) * 0.5f;
   else if (h1p + h2p  <  360.0f)   hbarp = (h1p + h2p + 360.0f) * 0.5f;
   else                               hbarp = (h1p + h2p - 360.0f) * 0.5f;

   float T = 1.0f
      - 0.17f * cos(to_rad(      hbarp - 30.0f))
      + 0.24f * cos(to_rad(2.0f * hbarp        ))
      + 0.32f * cos(to_rad(3.0f * hbarp +  6.0f))
      - 0.20f * cos(to_rad(4.0f * hbarp - 63.0f));

   float SL = 1.0f + 0.015f * (Lbarp - 50.0f) * (Lbarp - 50.0f)
                             / sqrt(20.0f + (Lbarp - 50.0f) * (Lbarp - 50.0f));
   float SC = 1.0f + 0.045f * Cbarp;
   float SH = 1.0f + 0.015f * Cbarp * T;

   float Cbarp7 = pow(Cbarp, 7.0f);
   float RC = 2.0f * sqrt(Cbarp7 / (Cbarp7 + 6103515625.0f));
   float dTheta = 30.0f * exp(-((hbarp - 275.0f) / 25.0f) * ((hbarp - 275.0f) / 25.0f));
   float RT = -sin(to_rad(2.0f * dTheta)) * RC;

   float tL = dLp / SL;
   float tC = dCp / SC;
   float tH = dHp / SH;
   return sqrt(tL*tL + tC*tC + tH*tH + RT * tC * tH);
}

// Per-image ΔE₀₀ statistics returned by compare_images.
struct image_diff
{
   double mean;  // mean ΔE across all pixels
   double p95;   // 95th-percentile ΔE  (5% of pixels may exceed this)
   double p99;   // 99th-percentile ΔE  (1% of pixels may exceed this)
};

// Compute per-pixel CIEDE2000 error and return summary statistics.
image_diff compare_images(uint32_t const* img1, uint32_t const* img2,
                          int width, int height)
{
   int n = width * height;
   std::vector<float> errors(n);
   for (int i = 0; i < n; ++i)
   {
      float L1, a1, b1, L2, a2, b2;
      pixel_to_lab(img1[i], L1, a1, b1);
      pixel_to_lab(img2[i], L2, a2, b2);
      errors[i] = delta_e_00(L1, a1, b1, L2, a2, b2);
   }
   std::sort(errors.begin(), errors.end());
   double sum = 0;
   for (float e : errors) sum += e;
   return {
      sum / n,
      errors[std::size_t(n * 0.95)],
      errors[std::size_t(n * 0.99)]
   };
}

// Luminance variance of a tile in img (measures local complexity).
// High variance → many edges/details; low variance → near-uniform region.
static float tile_variance(uint32_t const* img, int width,
                           int x0, int y0, int tw, int th)
{
   double sum = 0, sum_sq = 0;
   int n = tw * th;
   for (int y = 0; y < th; ++y)
   {
      uint32_t const* row = img + (y0 + y) * width + x0;
      for (int x = 0; x < tw; ++x)
      {
         uint32_t px = row[x];
         float R = (px >> 16) & 0xFF;
         float G = (px >>  8) & 0xFF;
         float B =  px        & 0xFF;
         float lum = 0.2126f * R + 0.7152f * G + 0.0722f * B;
         sum    += lum;
         sum_sq += lum * lum;
      }
   }
   double mean = sum / n;
   return float(sum_sq / n - mean * mean);
}

// Mean ΔE₀₀ for one tile.
static float tile_mean_de(uint32_t const* img1, uint32_t const* img2,
                          int width, int x0, int y0, int tw, int th)
{
   double sum = 0;
   for (int y = 0; y < th; ++y)
   {
      uint32_t const* r1 = img1 + (y0 + y) * width + x0;
      uint32_t const* r2 = img2 + (y0 + y) * width + x0;
      for (int x = 0; x < tw; ++x)
      {
         float L1, a1, b1, L2, a2, b2;
         pixel_to_lab(r1[x], L1, a1, b1);
         pixel_to_lab(r2[x], L2, a2, b2);
         sum += delta_e_00(L1, a1, b1, L2, a2, b2);
      }
   }
   return float(sum / (tw * th));
}

// Complexity-adaptive tiled ΔE check.
//
// Tiles are 32×32. Complexity is the luminance variance of the golden tile:
//   simple   (var <   50) → uniform areas, solid fills       → tight threshold
//   moderate (var <  500) → gradients, anti-aliased edges    → medium threshold
//   complex  (var >= 500) → detailed shapes, text, gradients → relaxed threshold
//
// Returns the worst (tile_de - threshold) margin across all tiles.
// Negative = all tiles pass; positive = at least one tile failed.
static float check_tiles(uint32_t const* golden, uint32_t const* result,
                         int width, int height,
                         int tile_size = 32)
{
   constexpr float k_simple   = 0.5f;   // ΔE tolerance for uniform tiles
   constexpr float k_moderate = 2.0f;   // ΔE tolerance for edge/gradient tiles
   constexpr float k_complex  = 5.0f;   // ΔE tolerance for detailed tiles

   float worst_margin = std::numeric_limits<float>::lowest();

   for (int ty = 0; ty < height; ty += tile_size)
   {
      for (int tx = 0; tx < width; tx += tile_size)
      {
         int tw = std::min(tile_size, width  - tx);
         int th = std::min(tile_size, height - ty);

         float var = tile_variance(golden, width, tx, ty, tw, th);
         float threshold = var < 50.0f  ? k_simple
                         : var < 500.0f ? k_moderate
                                        : k_complex;

         float de = tile_mean_de(golden, result, width, tx, ty, tw, th);
         float margin = de - threshold;
         if (margin > worst_margin) worst_margin = margin;
      }
   }
   return worst_margin;
}

void compare_golden(image const& pm, std::string name)
{
   pm.save_png(get_results_path() + name + ".png");
   auto golden = image(get_golden_path() + name + ".png");
   auto result = image(get_results_path() + name + ".png");

   auto bm_size = result.bitmap_size();
   CHECK(bm_size == golden.bitmap_size());

   auto size = result.size();
   CHECK(size.x == Approx(window_size.x));
   CHECK(size.y == Approx(window_size.y));

   auto a = golden.pixels();
   auto b = result.pixels();

   REQUIRE(a != nullptr);
   REQUIRE(b != nullptr);

   auto diff   = compare_images(a, b, bm_size.x, bm_size.y);
   float margin = check_tiles(a, b, bm_size.x, bm_size.y);

   std::cout << std::fixed << std::setprecision(4)
      << "ΔE  mean=" << diff.mean
      << "  p95="    << diff.p95
      << "  p99="    << diff.p99
      << "  tile_margin=" << margin
      << "  (" << name << ")\n";

   CHECK(diff.mean  < 1.0);
   CHECK(diff.p95   < 3.0);
   CHECK(margin     < 0.0f);
}

void typography(canvas& cnv)
{
   background(cnv);

   cnv.fill_style(rgba(220, 220, 220, 200));
   cnv.stroke_style(rgba(220, 220, 220, 200));

   // Regular
   cnv.font(font_descr{"Open Sans", 36});
   cnv.fill_text("Regular", 20, 40);

   // Bold
   cnv.font(font_descr{"Open Sans", 36}.bold());
   cnv.fill_text("Bold", 160, 40);

   // Light
   cnv.font(font_descr{"Open Sans", 36}.light());
   cnv.fill_text("Light", 250, 40);

   // Italic
   cnv.font(font_descr{"Open Sans", 36}.italic());
   cnv.fill_text("Italic", 345, 40);

   // Condensed
   cnv.font(font_descr{"Open Sans Condensed, Open Sans", 36}.condensed());
   cnv.fill_text("Condensed", 430, 40);

   // Condensed Italic
   cnv.font(font_descr{"Open Sans Condensed, Open Sans", 36}.italic().condensed());
   cnv.fill_text("Condensed Italic", 20, 115);

   // In the last two cases, the font family 'Open Sans Condensed' already
   // describes the font as condensed, but we still add the font family 'Open
   // Sans' and styles because, depending on platform, the font family 'Open
   // Sans Condensed' is either grouped into the 'Open Sans' family (e.g.
   // Windows), or separate as a distinct family (e.g. MacOS).

   // Outline
   cnv.font(font_descr{"Open Sans", 36}.bold());
   cnv.line_width(0.5);
   cnv.stroke_text("Outline", 210, 115);

   cnv.font(font_descr{"Open Sans", 52}.bold());

   // Gradient Fill
   {
      auto gr = canvas::linear_gradient{{360, 90}, {360, 140}};
      gr.add_color_stop(0.0, colors::navy_blue);
      gr.add_color_stop(1.0, colors::maroon);
      cnv.fill_style(gr);
      cnv.fill_text("Gradient", 360, 115);
      cnv.stroke_text("Gradient", 360, 115);
   }

   // Outline Gradient Fill
   {
      auto gr = canvas::linear_gradient{{360, 165}, {360, 215}};
      gr.add_color_stop(0.0, colors::medium_blue);
      gr.add_color_stop(1.0, colors::medium_violet_red);
      cnv.line_width(1.5);
      cnv.stroke_style(gr);
      cnv.stroke_text("Outline Gradient", 20, 190);
   }

#if defined(ARTIST_QUARTZ_2D) // CoreText supports ligatures right out of the box, but only for some fonts
   cnv.font(font_descr{"Lucida Grande", 52}.bold());
#else
   cnv.font(font_descr{"Open Sans", 52}.bold());
#endif

   cnv.fill_style(rgba(220, 220, 220, 200));
   cnv.fill_text("fi fl", 500, 190);

   cnv.font(font_descr{"Open Sans", 52}.weight(semi_bold));
   {
      auto state = cnv.new_state();

      // Shadow
      cnv.fill_style(rgba(220, 220, 220, 200));
      cnv.shadow_style({5.0, 5.0}, 5, colors::black);
      cnv.fill_text("Shadow", 20, 265);

      // Glow
      cnv.fill_style(bkd_color);
      cnv.shadow_style(8, colors::light_sky_blue);
      cnv.fill_text("Glow", 250, 265);

      auto m = cnv.measure_text("Shadow");
      CHECK(std::floor(m.ascent) == 55);
      CHECK(std::floor(m.descent) == 15);
      CHECK(std::floor(m.leading) == 0);
      CHECK(std::floor(m.size.x) == 198);
      CHECK(std::floor(m.size.y) == 70);
   }

   cnv.move_to({500, 220});
   cnv.line_to({500, 480});
   cnv.stroke_style(colors::red);
   cnv.line_width(0.5);
   cnv.stroke();

   cnv.fill_style(rgba(220, 220, 220, 200));
   cnv.font(font_descr{"Open Sans", 14});

   char const* align_text[] = {
      "text_align(left)",
      "text_align(right)",
      "text_align(center)",
      "text_align(baseline)",
      "text_align(top)",
      "text_align(middle)",
      "text_align(bottom)"
   };

   int aligns[] = {
      cnv.left,
      cnv.right,
      cnv.center,
      cnv.baseline,
      cnv.top,
      cnv.middle,
      cnv.bottom
   };

   float vspace = 35;
   float vstart = 250-vspace;
   for (int i = 0; i != 7; ++i)
   {
      vstart += vspace;
      cnv.move_to({400, vstart});
      cnv.line_to({600, vstart});
      cnv.stroke();
      cnv.text_align(aligns[i]);
      cnv.fill_text(align_text[i], 500, vstart);
   }

   std::string text =
      "Although I am a typical loner in daily life, my consciousness of "
      "belonging to the invisible community of those who strive for "
      "truth, beauty, and justice has preserved me from feeling isolated.\n\n"

      "The years of anxious searching in the dark, with their intense "
      "longing, their alternations of confidence and exhaustion, and "
      "final emergence into light—only those who have experienced it "
      "can understand that.\n\n"

      "⁠—Albert Einstein"
      ;

   auto tlayout = text_layout{
      font_descr{"Open Sans", 12}.italic()
    , text
   };
   tlayout.flow(350, true);
   tlayout.draw(cnv, 20, 300, rgba(220, 220, 220, 200));

   // Hit testing
   {
      auto i = tlayout.caret_index(0, 2000);
      CHECK(i == tlayout.npos);

      i = tlayout.caret_index(0, 0);
      CHECK(i == 0);

      i = tlayout.caret_index(350, 0);
      CHECK(i == 64);
      CHECK(text[i] == ' ');

      i = tlayout.caret_index(0, 20);
      CHECK(i == 133);
      CHECK(text[i] == 'b');

      i = tlayout.caret_index(5, 20);
      CHECK(i == 134);
      CHECK(text[i] == 'e');

      i = tlayout.caret_index(350, 20);
      CHECK(i == 192);
      CHECK(text[i] == '\n');

      i = tlayout.caret_index(109, 20);
      CHECK(i == 154);
      CHECK(text[i] == 'a');

      i = tlayout.caret_index(350, 15);
      CHECK(i == 132);
      CHECK(text[i] == ' ');

      // Harfbuzz vs. CoreText have slightly different results here,
      // but that is OK.
      i = tlayout.caret_index(343, 15);
      auto check_index = i == 131 || i == 130;
      auto check_char = text[i] == ',' || text[i] == 'h';
      CHECK(check_index);
      CHECK(check_char);

      i = tlayout.caret_index(20, 49);
      CHECK(i == 193);
      CHECK(text[i] == '\n');

      i = tlayout.caret_index(0, 147);
      CHECK(i == 403);
      char32_t cp = *(tlayout.text().data()+i);
      CHECK(cp == 8288); // 'WORD JOINER' (U+2060)

      i = tlayout.caret_index(88, 147);
      CHECK(i == tlayout.text().size());
   }

   // glyph_bounds
   {
      auto test_caret_pos =
         [&tlayout](std::size_t index, bool exceed = false)
         {
            point pos = tlayout.caret_point(index);
            if (exceed)
            {
               CHECK(std::floor(pos.x) == 86);
               CHECK(std::floor(pos.y) == 147);
            }
            else
            {
               std::size_t got_index = tlayout.caret_index(pos);
               CHECK(got_index == index);
            }
         };

      test_caret_pos(1000, true);
      test_caret_pos(-1, true);

      test_caret_pos(10);
      test_caret_pos(0);
      test_caret_pos(64);
      test_caret_pos(193);
      test_caret_pos(132);
      test_caret_pos(133);
      test_caret_pos(154);
      test_caret_pos(325);
      test_caret_pos(326);
      test_caret_pos(405);
      test_caret_pos(420);
   }
}

char const* mode_name(canvas::composite_op_enum mode)
{
   switch (mode)
   {
      case canvas::source_over:       return "source_over";
      case canvas::source_atop:       return "source_atop";
      case canvas::source_in:         return "source_in";
      case canvas::source_out:        return "source_out";

      case canvas::destination_over:  return "destination_over";
      case canvas::destination_atop:  return "destination_atop";
      case canvas::destination_in:    return "destination_in";
      case canvas::destination_out:   return "destination_out";

      case canvas::lighter:           return "lighter";
      case canvas::darker:            return "darker";
      case canvas::copy:              return "copy";
      case canvas::xor_:              return "xor_";

      case canvas::difference:        return "difference";
      case canvas::exclusion:         return "exclusion";
      case canvas::multiply:          return "multiply";
      case canvas::screen:            return "screen";

      case canvas::color_dodge:       return "color_dodge";
      case canvas::color_burn:        return "color_burn";
      case canvas::soft_light:        return "soft_light";
      case canvas::hard_light:        return "hard_light";

      case canvas::hue:               return "hue";
      case canvas::saturation:        return "saturation";
      case canvas::color_op:          return "color_op";
      case canvas::luminosity:        return "luminosity";

      default: return "";
   };
}

void composite_draw(canvas& cnv, point p, canvas::composite_op_enum mode)
{
   {
      auto save = cnv.new_state();
      cnv.add_rect({p.x, p.y, p.x + 120, p.y + 130});
      cnv.clip();

      cnv.global_composite_operation(cnv.source_over);
      cnv.fill_style(colors::blue);
      cnv.fill_rect(p.x+20, p.y+20, 60, 60);
   }

   {
      auto save = cnv.new_state();
      image pm{110, 110};
      {
         offscreen_image ctx{pm};
         canvas pm_cnv{ctx.context()};
         pm_cnv.fill_style(colors::red);
         pm_cnv.add_circle(70, 70, 30);
         pm_cnv.fill();
      }

      cnv.global_composite_operation(mode);
      cnv.draw(pm, p);
   }

   cnv.fill_style(colors::black);
   cnv.text_align(cnv.center);
   cnv.fill_text(mode_name(mode), p.x+60, p.y+110);
}

void composite_ops(canvas& cnv)
{
   cnv.font(font_descr{"Open Sans", 10});

   composite_draw(cnv, {0, 0}, cnv.source_over);
   composite_draw(cnv, {120, 0}, cnv.source_atop);
   composite_draw(cnv, {240, 0}, cnv.source_in);
   composite_draw(cnv, {360, 0}, cnv.source_out);

   composite_draw(cnv, {0, 120}, cnv.destination_over);
   composite_draw(cnv, {120, 120}, cnv.destination_atop);
   composite_draw(cnv, {240, 120}, cnv.destination_in);
   composite_draw(cnv, {360, 120}, cnv.destination_out);

   composite_draw(cnv, {0, 240}, cnv.lighter);
   composite_draw(cnv, {120, 240}, cnv.darker);
   composite_draw(cnv, {240, 240}, cnv.copy);
   composite_draw(cnv, {360, 240}, cnv.xor_);
}

void composite_ops2(canvas& cnv)
{
   cnv.font(font_descr{"Open Sans", 10});

   composite_draw(cnv, {0, 0},   cnv.difference);
   composite_draw(cnv, {120, 0}, cnv.exclusion);
   composite_draw(cnv, {240, 0}, cnv.multiply);
   composite_draw(cnv, {360, 0}, cnv.screen);

   composite_draw(cnv, {0, 130},   cnv.color_dodge);
   composite_draw(cnv, {120, 130}, cnv.color_burn);
   composite_draw(cnv, {240, 130}, cnv.soft_light);
   composite_draw(cnv, {360, 130}, cnv.hard_light);

   composite_draw(cnv, {0, 260},   cnv.hue);
   composite_draw(cnv, {120, 260}, cnv.saturation);
   composite_draw(cnv, {240, 260}, cnv.color_op);
   composite_draw(cnv, {360, 260}, cnv.luminosity);
}

void drop_shadow(canvas& cnv)
{
   cnv.shadow_style({20, 20}, 10, colors::black);
   cnv.fill_style(colors::red);
   cnv.fill_rect(20, 20, 100, 80);

   cnv.scale(2, 2);
   cnv.translate(60, 0);
   cnv.shadow_style({20, 20}, 10, colors::black);
   cnv.fill_style(colors::blue);
   cnv.fill_rect(20, 20, 100, 80);
}

auto constexpr tauri_bkd = rgb(44, 42, 45);

void tauri_logo(canvas& cnv)
{
   cnv.begin_path();
   cnv.move_to(24.091255, 16.26616);
   cnv.line_to(44.374182, 43);
   cnv.bezier_curve_to(44.374182, 43, 34.488091, 43.011495, 34.488091, 43.011495);
   cnv.bezier_curve_to(34.488091, 43.011495, 35.642931, 41.930399, 36.249098, 41.25);
   cnv.bezier_curve_to(37.686127, 39.636989, 37.287291, 38.778661, 31.154085, 30.285105);
   cnv.bezier_curve_to(27.494338, 25.216912, 24.275, 21.070209, 24, 21.070209);
   cnv.bezier_curve_to(23.725, 21.070209, 20.505662, 25.216912, 16.845915, 30.285105);
   cnv.bezier_curve_to(10.712709, 38.778661, 10.313873, 39.636989, 11.750902, 41.25);
   cnv.bezier_curve_to(12.342655, 41.91422, 13.259451, 42.993021, 13.259451, 42.993021);
   cnv.line_to(3.6258176, 43);
   cnv.bezier_curve_to(3.6258176, 43, 24.091255, 16.26616, 24.091255, 16.26616);
   cnv.fill();
   cnv.begin_path();
   cnv.line_width(2.5);
   cnv.add_circle(24, 8, 4);
   cnv.stroke();
}

void tauri(canvas& cnv)
{
   cnv.add_rect({{0, 0}, window_size});
   cnv.fill_style(tauri_bkd);
   cnv.fill();
   cnv.scale(10, 10);
   cnv.translate(9, 0);

   // Glow
   cnv.fill_style(tauri_bkd);
   cnv.stroke_style(tauri_bkd);
   cnv.shadow_style({-1, -1}, 10, colors::light_cyan);
   tauri_logo(cnv);

   // Shadow
   cnv.fill_style(tauri_bkd);
   cnv.stroke_style(tauri_bkd);
   cnv.shadow_style({5, 5}, 20, rgb(0, 0, 20));
   tauri_logo(cnv);

   // Gradient
   auto gr = canvas::linear_gradient{0, 0, 50, 50};
   gr.add_color_stop(0.0, colors::gold);
   gr.add_color_stop(1.0, colors::gold.opacity(0));
   cnv.fill_style(gr);
   cnv.stroke_style(gr);
   tauri_logo(cnv);
}

void paths(canvas& cnv)
{
   auto stroke_fill =
      [&](path const& p, color fill_c, color stroke_c)
      {
         cnv.add_path(p);
         cnv.fill_color(fill_c);
         cnv.stroke_color(stroke_c);
         cnv.fill();
         cnv.line_width(5);
         cnv.add_path(p);
         cnv.stroke();
      };

   auto stroke =
      [&](path const& p, color stroke_c)
      {
         cnv.add_path(p);
         cnv.stroke_color(stroke_c);
         cnv.line_width(5);
         cnv.stroke();
      };

   auto dot =
      [&](float x, float y)
      {
         cnv.add_circle(x, y, 10);
         cnv.fill_color(colors::white.opacity(0.5));
         cnv.fill();
      };

   background(cnv);

   // These paths are defined using SVG-style strings lifted off the
   // W3C SVG documentation: https://www.w3.org/TR/SVG/paths.html

   {
      auto save = cnv.new_state();
      cnv.scale(0.5, 0.5);

      cnv.translate(-40, 0);
      path p1{"M 100 100 L 300 100 L 200 300 z"};
      stroke_fill(p1, colors::green.opacity(0.5), colors::ivory);

      cnv.translate(220, 0);
      path p2{"M100,200 C100,100 250,100 250,200 S400,300 400,200"};
      stroke(p2, colors::light_sky_blue);

      cnv.translate(-150, 250);
      path p3{"M200,300 Q400,50 600,300 T1000,300"};
      stroke(p3, colors::light_sky_blue);

      path p4{"M200,300 L400,50 L600,300 L800,550 L1000,300"};
      stroke(p4, colors::light_gray.opacity(0.5));
      dot(200, 300);
      dot(600, 300);
      dot(1000, 300);
      dot(400, 50);
      dot(800, 550);
      cnv.translate(150, -250);

      cnv.translate(350, 0);
      path p5{"M300,200 h-150 a150,150 0 1,0 150,-150 z"};
      stroke_fill(p5, colors::red.opacity(0.8), colors::ivory);

      path p6{"M275,175 v-150 a150,150 0 0,0 -150,150 z"};
      stroke_fill(p6, colors::blue.opacity(0.8), colors::ivory);

      cnv.translate(-350, 200);
      path p7{
         "M600,350 l 50,-25"
         "a25,25 -30 0,1 50,-25 l 50,-25"
         "a25,50 -30 0,1 50,-25 l 50,-25"
         "a25,75 -30 0,1 50,-25 l 50,-25"
         "a25,100 -30 0,1 50,-25 l 50,-25"
      };
      stroke(p7, colors::ivory);
   }
}

void compare_transform(affine_transform const& a, affine_transform const& b)
{
   CHECK(a.a == Approx(b.a));
   CHECK(a.b == Approx(b.b));
   CHECK(a.c == Approx(b.c));
   CHECK(a.d == Approx(b.d));
   CHECK(a.tx == Approx(b.tx));
   CHECK(a.ty == Approx(b.ty));
}

void misc(canvas& cnv)
{
   using cycfi::pi;

   background(cnv);
   cnv.clear_rect(30, 30, 50, 50);

   {
      auto save = cnv.new_state();

      affine_transform mat = cnv.transform();
      cnv.fill_style(colors::blue);

      cnv.rotate(pi/4);
      mat = mat.rotate(pi/4);
      auto ctm = cnv.transform();
      compare_transform(mat, ctm);

      cnv.scale(2);
      mat = mat.scale(2);
      ctm = cnv.transform();
      compare_transform(mat, ctm);

      cnv.translate(100, 0);
      mat = mat.translate(100, 0);
      ctm = cnv.transform();
      compare_transform(mat, ctm);

      cnv.add_rect(-25, -25, 50, 50);
      cnv.fill();

      cnv.transform(mat);
      cnv.add_rect(-20, -20, 40, 40);
      cnv.fill_style(colors::red);
      cnv.fill();
   }

   {
      affine_transform mat;
      auto p = mat.apply(0.0, 0.0);
      CHECK(p.x == Approx(0));
      CHECK(p.y == Approx(0));

      mat = mat.scale(10);
      p = mat.apply(4, 4);
      CHECK(p.x == Approx(40));
      CHECK(p.y == Approx(40));

      mat = mat.translate(2, 2);
      p = mat.apply(0.0, 0.0);
      CHECK(p.x == Approx(20));
      CHECK(p.y == Approx(20));
   }

   {
      auto save = cnv.new_state();

      affine_transform mat = cnv.transform();
      mat = mat.translate(144, 144);
      mat = mat.skew(pi/8, pi/12);
      cnv.transform(mat);
      cnv.add_rect(0, 0, 72, 72);
      cnv.fill_style(colors::green);
      cnv.fill();
   }

   {
      path p;
      p.add_circle(circle{230, 230, 50});
      p.add_circle(circle{230, 230, 25});
      p.fill_rule(path::fill_odd_even);

      CHECK(p.includes(230-50+5, 230));
      CHECK(!p.includes(230, 230));

      cnv.clip(p);
      cnv.add_rect(0, 0, 500, 500);
      cnv.fill_style(colors::navajo_white.opacity(0.5));
      cnv.fill_preserve();

      CHECK(cnv.point_in_path(10, 10));
      CHECK(!p.includes(501, 500));
   }

   // Test small offscreen hit testing and text measurements work
   {
      image img{1, 1};
      offscreen_image offscr{img};
      canvas cnv{offscr.context()};

      cnv.add_circle(230, 230, 50);
      cnv.add_circle(230, 230, 25);
      cnv.fill_rule(path::fill_odd_even);

      CHECK(cnv.point_in_path(230-50+5, 230));
      CHECK(!cnv.point_in_path(230, 230));

      cnv.font(font_descr{"Open Sans", 36});
      auto m = cnv.measure_text("Hello, World");
      CHECK(std::abs(m.size.x-205.0f) <= 1.0);
      CHECK(std::abs(m.size.y-49) <= 1.0);
      CHECK(std::floor(m.ascent) == 38);
      CHECK(std::floor(m.descent) == 10);
      CHECK(std::floor(m.leading) == 0);
   }
}

///////////////////////////////////////////////////////////////////////////////
namespace cycfi::artist
{
   void init_paths()
   {}

   // This is declared in font.hpp
   fs::path get_user_fonts_directory()
   {
      return get_fonts_path();
   }
}

///////////////////////////////////////////////////////////////////////////////
// Test Drivers
///////////////////////////////////////////////////////////////////////////////

// Visual regression testing scheme
// ---------------------------------
// Each visual test renders into an offscreen image and compares it against a
// stored golden PNG using two complementary perceptual metrics.
//
// 1. Global ΔE₀₀ stats (compare_images)
//    Converts every pixel to CIELAB (via sRGB → linear → XYZ → LAB) and
//    computes CIEDE2000 perceptual colour difference.  The mean and 95th-
//    percentile ΔE across all pixels must stay below loose global thresholds.
//    This catches large-area defects: wrong colours, wrong transforms, wrong
//    composite operations.
//      mean ΔE  < 1.0   (imperceptible averaged over the whole image)
//      p95  ΔE  < 3.0   (95% of pixels within just-noticeable difference)
//
// 2. Complexity-adaptive tiled check (check_tiles)
//    The image is divided into 32×32 pixel tiles.  Each tile's mean ΔE must
//    not exceed a threshold that scales with the tile's luminance variance in
//    the golden — a proxy for local rendering complexity:
//
//      Simple   (var <   50)  tolerance 0.5  — solid fills, uniform backgrounds.
//                                              Near pixel-perfect required.
//      Moderate (var <  500)  tolerance 2.0  — gradients, anti-aliased edges.
//      Complex  (var >= 500)  tolerance 5.0  — dense shapes, text, fine detail.
//
//    The tight tolerance on simple (low-variance) tiles is the key property:
//    a handful of shifted or recoloured pixels on a solid background exceeds
//    0.5 ΔE and fails, even though the same change is invisible to the global
//    mean.  This is what makes the scheme sensitive to small, localised
//    regressions (e.g. a shifted glyph, a missing stroke) without generating
//    false positives in legitimately complex regions.
//
// Updating goldens
//    1. Copy one of the test cases below.
//    2. Point it at your new draw function.
//    3. Run — the first run fails because no golden exists yet.
//    4. Inspect the PNG written to the "results" directory in the CMake build
//       output folder.
//    5. If the output looks correct, copy it to the golden directory next to
//       the source files (e.g. test/macos_golden/cairo/).
//    6. Re-run CMake to pick up the new file, then confirm the test passes.
//    Note: goldens are platform- and backend-specific.  Regenerate a golden
//    whenever intentional rendering changes cause a legitimate difference; do
//    not lower the thresholds to paper over a regression.

TEST_CASE("Drawing")
{
   image pm{window_size};
   {
      offscreen_image ctx{pm};
      canvas pm_cnv{ctx.context()};
      test_draw(pm_cnv);
   }
   compare_golden(pm, "shapes_and_images");
}

TEST_CASE("Drawing2")
{
   image pm{window_size};
   {
      offscreen_image ctx{pm};
      canvas pm_cnv{ctx.context()};
      test_draw2(pm_cnv);
   }
   compare_golden(pm, "shapes2");
}

TEST_CASE("Typography")
{
   image pm{window_size};
   {
      offscreen_image ctx{pm};
      canvas pm_cnv{ctx.context()};

      auto p = pm_cnv.device_to_user(100, 100);

      typography(pm_cnv);
   }
   compare_golden(pm, "typography");
}

TEST_CASE("Composite")
{
   image pm{window_size};
   {
      offscreen_image ctx{pm};
      canvas pm_cnv{ctx.context()};
      composite_ops(pm_cnv);
   }
   compare_golden(pm, "composite_ops");
}

TEST_CASE("Composite2")
{
   image pm{window_size};
   {
      offscreen_image ctx{pm};
      canvas pm_cnv{ctx.context()};
      composite_ops2(pm_cnv);
   }
   compare_golden(pm, "composite_ops2");
}

TEST_CASE("DropShadow")
{
   image pm{window_size};
   {
      offscreen_image ctx{pm};
      canvas pm_cnv{ctx.context()};
      drop_shadow(pm_cnv);
   }
   compare_golden(pm, "drop_shadow");
}

TEST_CASE("Tauri")
{
   image pm{window_size};
   {
      offscreen_image ctx{pm};
      canvas pm_cnv{ctx.context()};
      tauri(pm_cnv);
   }
   compare_golden(pm, "tauri");
}

TEST_CASE("Paths")
{
   image pm{window_size};
   {
      offscreen_image ctx{pm};
      canvas pm_cnv{ctx.context()};
      paths(pm_cnv);
   }
   compare_golden(pm, "paths");
}

TEST_CASE("Misc")
{
   image pm{window_size};
   {
      offscreen_image ctx{pm};
      canvas pm_cnv{ctx.context()};
      misc(pm_cnv);
   }
   compare_golden(pm, "misc");
}


void chessboard(canvas& cnv)
{
   int constexpr rows = 8;
   int constexpr cols = 8;
   int constexpr square_side = 60;
   int constexpr square_area = square_side * square_side;
   size_t constexpr pix_buf_size = rows * cols * square_area;

   uint32_t constexpr white = 0xffffffff;
   auto black = []() -> uint32_t { return cycfi::is_little_endian() ? 0xff000000 : 0x000000ff; };

   std::unique_ptr<uint32_t[]> pix_buf(new uint32_t[pix_buf_size]);
   for (int y = 0; y < rows; y++)
   {
      uint32_t* row_slice = &pix_buf[y * cols * square_area];
      uint32_t color = (y % 2 != 0) ? white : black();
      for (int x = 0; x < cols * square_area; x++)
      {
         if (x % square_side == 0)
            color = (color == white) ? black() : white;
         row_slice[x] = color;
      }
   }

   auto img = make_image<pixel_format::rgba32>(
      pix_buf.get(), {float(cols * square_side), float(rows * square_side)});
   cnv.draw(img, {0, 0});
}

TEST_CASE("Chessboard")
{
   image pm{window_size};
   {
      offscreen_image ctx{pm};
      canvas pm_cnv{ctx.context()};
      chessboard(pm_cnv);
   }
   compare_golden(pm, "chessboard");
}

TEST_CASE("Scale and Coordinate Conversion")
{
   // Verify device_to_user / user_to_device are inverses of each other
   // for a plain offscreen image surface (identity initial CTM).
   image pm{window_size};
   {
      offscreen_image ctx{pm};
      canvas pm_cnv{ctx.context()};

      // With no transform: device coords == user coords.
      auto d = pm_cnv.user_to_device({100.0f, 200.0f});
      auto u = pm_cnv.device_to_user(d);
      CHECK(u.x == Approx(100.0f).epsilon(0.001));
      CHECK(u.y == Approx(200.0f).epsilon(0.001));

      // After applying a scale transform: verify inverse round-trip.
      pm_cnv.save();
      pm_cnv.scale(2.0f, 2.0f);

      auto d2 = pm_cnv.user_to_device({50.0f, 75.0f});
      auto u2 = pm_cnv.device_to_user(d2);
      CHECK(u2.x == Approx(50.0f).epsilon(0.001));
      CHECK(u2.y == Approx(75.0f).epsilon(0.001));

      // After scaling by 2, device coords should be 2x the user coords.
      CHECK(d2.x == Approx(100.0f).epsilon(0.001));
      CHECK(d2.y == Approx(150.0f).epsilon(0.001));

      pm_cnv.restore();
   }
}

TEST_CASE("Coordinate Conversion with Base Transform")
{
   // A host may hand the canvas a context whose CTM already encodes a base
   // transform (e.g. the GTK content-area / window-decoration offset).
   // device_to_user / user_to_device must be relative to that baseline so the
   // view origin maps to the host drawing origin -- not shifted by the base
   // offset.  (Regression: the Cairo backend used the absolute cairo_*_user
   // conversions, shifting all content by the decoration offset.)
   image pm{window_size};
   {
      offscreen_image ctx{pm};

      // Establish a non-identity base transform on the context, then build the
      // canvas that elements would draw through.  The constructor captures this
      // as its baseline.
      canvas base{ctx.context()};
      base.translate({26.0f, 60.0f});

      canvas pm_cnv{ctx.context()};

      // The view origin (0,0) maps to the host drawing origin, relative to the
      // captured baseline -- i.e. (0,0), not (26,60).
      auto origin = pm_cnv.user_to_device({0.0f, 0.0f});
      CHECK(origin.x == Approx(0.0f).margin(0.001));
      CHECK(origin.y == Approx(0.0f).margin(0.001));

      auto back = pm_cnv.device_to_user({0.0f, 0.0f});
      CHECK(back.x == Approx(0.0f).margin(0.001));
      CHECK(back.y == Approx(0.0f).margin(0.001));

      // An arbitrary point is unaffected by the base offset and round-trips.
      auto d = pm_cnv.user_to_device({40.0f, 70.0f});
      CHECK(d.x == Approx(40.0f).margin(0.001));
      CHECK(d.y == Approx(70.0f).margin(0.001));

      auto u = pm_cnv.device_to_user(d);
      CHECK(u.x == Approx(40.0f).margin(0.001));
      CHECK(u.y == Approx(70.0f).margin(0.001));
   }
}

TEST_CASE("Text Shaping")
{
   image pm{window_size};
   offscreen_image ctx{pm};
   canvas cnv{ctx.context()};

   // Basic shaping sanity: positive advance, longer text wider than shorter.
   {
      cnv.font(font_descr{"Open Sans", 36});
      auto m1 = cnv.measure_text("A");
      auto m2 = cnv.measure_text("AA");
      auto m3 = cnv.measure_text("AAA");
      CHECK(m1.size.x > 0);
      CHECK(m2.size.x > m1.size.x);
      CHECK(m3.size.x > m2.size.x);
   }

   // Ligature / kern: "fi" must not be wider than "f"+"i" measured separately.
   // With HarfBuzz liga enabled, "fi" is typically narrower (ligature substitution).
   // Without liga it is still ≤ sum because no negative-kern penalty applies.
   {
      cnv.font(font_descr{"Open Sans", 36}.bold());
      auto mf  = cnv.measure_text("f");
      auto mi  = cnv.measure_text("i");
      auto mfi = cnv.measure_text("fi");
      CHECK(mf.size.x > 0);
      CHECK(mi.size.x > 0);
      CHECK(mfi.size.x <= mf.size.x + mi.size.x + 1.0f);  // 1px tolerance
   }

   // text_layout::num_lines() must reflect reflow width.
   // "Hello World" at Open Sans 14 fits on one line at 640px, two or more at 1px.
   {
      {
         text_layout tl{font_descr{"Open Sans", 14}, "Hello World"};
         tl.flow(640, false);
         CHECK(tl.num_lines() == 1);
      }
      {
         text_layout tl{font_descr{"Open Sans", 14}, "Hello World"};
         tl.flow(1, false);
         CHECK(tl.num_lines() >= 2);
      }
   }

   // Narrower flow always produces at least as many lines as wider flow.
   {
      std::string const txt =
         "The quick brown fox jumps over the lazy dog";
      text_layout wide {font_descr{"Open Sans", 14}, txt};
      text_layout narrow{font_descr{"Open Sans", 14}, txt};
      wide.flow(640, false);
      narrow.flow(100, false);
      CHECK(narrow.num_lines() >= wide.num_lines());
   }

   // caret_point(0) is the start of the first glyph (x near 0, y >= 0).
   {
      text_layout tl{font_descr{"Open Sans", 14}, "Hello"};
      tl.flow(640, false);
      auto p = tl.caret_point(0);
      CHECK(p.x >= 0.0f);
      CHECK(p.y >= 0.0f);
   }

   // caret_point advances monotonically left-to-right for simple ASCII.
   {
      std::string const txt = "Hello";
      text_layout tl{font_descr{"Open Sans", 14}, txt};
      tl.flow(640, false);
      float prev_x = -1.0f;
      for (std::size_t i = 0; i < txt.size(); ++i)
      {
         auto p = tl.caret_point(i);
         CHECK(p.x >= prev_x);
         prev_x = p.x;
      }
   }
}

TEST_CASE("Word Selection")
{
   // word_break(i) marks a UAX-29 word boundary AFTER character i.  This
   // reference selection mirrors the elements double-click handler: it returns
   // the [start, end) word containing a click.  It verifies the boundary data
   // drives correct whole-word selection across contractions, decimals,
   // grouping separators and punctuation.
   auto select = [](std::string const& txt, std::size_t click) -> std::string
   {
      text_layout tl{font_descr{"Open Sans", 14}, txt};
      std::size_t n = txt.size();
      auto is_wb = [&](std::size_t i)
         { return tl.word_break(i) == text_layout::allow_break; };
      std::size_t end = click;
      while (end < n && !is_wb(end)) ++end;
      if (end < n) ++end;                         // boundary is after char `end`
      std::size_t start = click;
      while (start > 0 && !is_wb(start - 1)) --start;
      return txt.substr(start, end - start);
   };

   CHECK(select("To traverse the", 5)  == "traverse");  // mid-word
   CHECK(select("To traverse the", 10) == "traverse");  // click last letter
   CHECK(select("To traverse the", 0)  == "To");        // first word
   CHECK(select("don't", 2)            == "don't");      // contraction kept
   CHECK(select("3.14", 1)             == "3.14");       // decimal kept
   CHECK(select("1,000", 3)            == "1,000");      // grouping kept
   CHECK(select("foo,bar", 1)          == "foo");        // punctuation boundary
   CHECK(select("foo,bar", 5)          == "bar");
   CHECK(select("e-mail", 3)           == "mail");       // hyphen boundary
   CHECK(select("resume", 2)           == "resume");
}

TEST_CASE("CJK line wrapping")
{
   // Issue cycfi/elements#430: CJK text without spaces that overflows the flow
   // width must wrap WITHOUT dropping characters.  Each ideograph is its own
   // line-break opportunity, so the break must keep the boundary glyph on the
   // line (unlike a space, which is consumed).  This also exercises the
   // force-break path, which previously read past the end of the glyph array.

   // 8 ideographs (one codepoint each), repeated 8 times => 64 codepoints.
   std::string cjk;
   for (int i = 0; i != 8; ++i)
      cjk += "开放包容视野宽广";
   std::size_t const n = 64;

   text_layout tl{font_descr{"Open Sans", 14}, cjk};
   tl.flow(40, false);                 // narrow: force many wraps

   // It must actually wrap into multiple lines.
   CHECK(tl.num_lines() >= 2);

   // Distinct row y-positions, in visual (top-to-bottom) order.
   std::vector<float> rows;
   for (std::size_t i = 0; i <= n; ++i)
   {
      float y = tl.caret_point(i).y;
      if (rows.empty() || y > rows.back())
         rows.push_back(y);
   }
   CHECK(rows.size() == tl.num_lines());

   // No character may be dropped at a wrap: the lines must cover a contiguous
   // run of characters (there is no whitespace between ideographs to consume).
   // The index one past the right edge of each line must equal the index at the
   // left edge of the next line, on every backend.
   for (std::size_t r = 0; r + 1 < rows.size(); ++r)
   {
      auto right = tl.caret_index(1e6f, rows[r]);
      auto left  = tl.caret_index(-1e6f, rows[r+1]);
      CHECK(right == left);
   }
}

TEST_CASE("Ligature at end of line")
{
   // Issue cycfi/elements#384: a string that ENDS in a ligature (e.g. the "fi"
   // of "wifi" shapes to a single glyph spanning two code points) made the whole
   // text disappear on the Skia backend.
   //
   // End-of-text is detected from libunibreak's INDETERMINATE marker, which sits
   // on the final code point.  When that code point is swallowed by a trailing
   // ligature it carries no glyph, so the marker was never seen and the final
   // line was never flushed -> num_lines() == 0 and nothing was drawn.

   text_layout tl{font_descr{"Open Sans", 14}, "wifi"};
   tl.flow(1000, false);               // wide: everything fits on one line

   // The line must be flushed: non-empty text always produces at least one line.
   CHECK(tl.num_lines() == 1);

   // The flowed line must have positive extent (the text is actually laid out,
   // not collapsed to nothing): the end-of-text caret sits to the right of the
   // start-of-text caret.
   auto start = tl.caret_point(0);
   auto end   = tl.caret_point(tl.text().size());
   CHECK(end.x > start.x);
   CHECK(start.y == end.y);            // single line
}

TEST_CASE("Ligature before a hard line break")
{
   // Issue cycfi/elements#384 (follow-up): a line ENDING in a ligature that is
   // immediately followed by a hard line break (e.g. "...ffl\n") must consume
   // the newline glyph, not draw it.  The end-of-text guard for trailing
   // ligatures must not also keep a trailing newline glyph, which renders as a
   // .notdef box at the right edge of the line.
   //
   // Detect it by rendering and scanning pixels: the first line of "Affl\nB"
   // must not extend further right than "Affl" drawn alone.  A leftover newline
   // box would push the rightmost inked pixel well past the ligature.

   auto rightmost_ink_first_line =
      [](char const* str) -> int
      {
         image img{300, 120};
         {
            offscreen_image offscr{img};
            canvas cnv{offscr.context()};
            cnv.add_rect(0, 0, 300, 120);
            cnv.fill_style(colors::white);
            cnv.fill();
            text_layout tl{font_descr{"Open Sans", 40}, str};
            tl.flow(290, false);
            tl.draw(cnv, {5, 45}, colors::black);
         }
         auto sz = img.bitmap_size();
         auto const* px = img.pixels();
         int w = int(sz.x), band = std::min(int(sz.y), 50);  // first line only
         int rightmost = -1;
         for (int y = 0; y != band; ++y)
            for (int x = 0; x != w; ++x)
            {
               // Any backend: white background is the max value in every
               // channel; inked (black) text lowers them.  Treat a clearly
               // non-white pixel as ink.
               auto p = px[y * w + x];
               unsigned b0 =  p        & 0xff;
               unsigned b1 = (p >>  8) & 0xff;
               unsigned b2 = (p >> 16) & 0xff;
               if (b0 < 200 || b1 < 200 || b2 < 200)
                  rightmost = std::max(rightmost, x);
            }
         return rightmost;
      };

   // The trailing newline must be the LAST glyph for the bug to bite (the user
   // pressed Return at the very end).  "Affl\n" ends in the ffl ligature then a
   // hard break that owns the final glyph.
   int plain   = rightmost_ink_first_line("Affl");
   int with_nl = rightmost_ink_first_line("Affl\n");

   CHECK(plain > 0);                            // text actually rendered
   CHECK(std::abs(with_nl - plain) <= 2);       // no extra newline box drawn

   // Font-independent check: a trailing newline contributes no width to its
   // line -- the newline glyph (a visible box in some fonts, a blank but
   // advancing glyph in others) must be consumed, not kept.  The right edge of
   // the line in "Affl\n" must equal the width of "Affl" alone; a kept newline
   // glyph pushes it out by that glyph's advance even when it draws no ink.
   {
      text_layout a{font_descr{"Open Sans", 40}, "Affl"};
      a.flow(290, false);
      text_layout b{font_descr{"Open Sans", 40}, "Affl\n"};
      b.flow(290, false);
      auto a_end = a.caret_point(a.text().size()).x;
      auto b_end = b.caret_point(b.text().size()).x;
      CHECK(std::abs(b_end - a_end) <= 1.0f);
   }
}

TEST_CASE("Path Equality")
{
   // Identical paths are equal
   {
      path a, b;
      a.add_rect(10, 10, 100, 50);
      b.add_rect(10, 10, 100, 50);
      CHECK(a == b);
   }

   // Same path object is equal to itself
   {
      path a;
      a.add_circle(50, 50, 30);
      CHECK(a == a);
   }

   // Different geometry is not equal
   {
      path a, b;
      a.add_rect(10, 10, 100, 50);
      b.add_rect(10, 10, 100, 51);  // height differs
      CHECK(!(a == b));
   }

   // Different shape type is not equal
   {
      path a, b;
      a.add_rect(10, 10, 80, 80);
      b.add_circle(50, 50, 40);
      CHECK(!(a == b));
   }

   // Different fill rule makes paths not equal
   {
      path a, b;
      a.add_circle(50, 50, 30);
      a.add_circle(50, 50, 15);
      b.add_circle(50, 50, 30);
      b.add_circle(50, 50, 15);
      a.fill_rule(path::fill_winding);
      b.fill_rule(path::fill_odd_even);
      CHECK(!(a == b));
   }

   // Same fill rule with same geometry is equal
   {
      path a, b;
      a.add_circle(50, 50, 30);
      a.add_circle(50, 50, 15);
      a.fill_rule(path::fill_odd_even);
      b.add_circle(50, 50, 30);
      b.add_circle(50, 50, 15);
      b.fill_rule(path::fill_odd_even);
      CHECK(a == b);
   }

   // SVG-parsed paths with identical strings are equal
   {
      path a{"M 100 100 L 300 100 L 200 300 z"};
      path b{"M 100 100 L 300 100 L 200 300 z"};
      CHECK(a == b);
   }

   // SVG-parsed paths with different strings are not equal
   {
      path a{"M 100 100 L 300 100 L 200 300 z"};
      path b{"M 100 100 L 300 100 L 200 301 z"};
      CHECK(!(a == b));
   }
}

TEST_CASE("Image Pixel Round Trip")
{
   // Opaque white pixels must survive make_image<rgba32> → pixels() intact.
   // White is premultiplied-invariant so this holds across all backends.
   {
      constexpr int w = 4, h = 4;
      uint32_t src[w * h];
      // RGBA32: R=255, G=255, B=255, A=255
      std::fill(src, src + w * h, uint32_t(0xFFFFFFFF));

      auto img = make_image<pixel_format::rgba32>(src, {float(w), float(h)});
      auto* pix = img.pixels();
      REQUIRE(pix != nullptr);
      for (int i = 0; i < w * h; ++i)
         CHECK(pix[i] == 0xFFFFFFFF);
   }

   // Opaque black must also round-trip correctly (A=255, RGB=0).
   // On little-endian, rgba32 uint32 bytes are [R,G,B,A], so black+opaque = 0xFF000000.
   {
      constexpr int w = 2, h = 2;
      // RGBA32 little-endian: bytes[R=0,G=0,B=0,A=255] → uint32 = 0xFF000000
      // Cairo ARGB32 output: A=255,R=0,G=0,B=0 → uint32 = 0xFF000000
      uint32_t src[w * h];
      std::fill(src, src + w * h, uint32_t(0xFF000000));

      auto img = make_image<pixel_format::rgba32>(src, {float(w), float(h)});
      auto* pix = img.pixels();
      REQUIRE(pix != nullptr);
      for (int i = 0; i < w * h; ++i)
         CHECK(pix[i] == 0xFF000000);
   }
}

TEST_CASE("Color Maths")
{
  color a(0.2, 0.25, 0.5, 1.0);
  color b(0.5, 0.6, 0.1, 1.0);

  auto check = [&](color result, color check)
  {
    REQUIRE_THAT(result.red, Catch::WithinRel(check.red, 0.001f));
    REQUIRE_THAT(result.green, Catch::WithinRel(check.green, 0.001f));
    REQUIRE_THAT(result.blue, Catch::WithinRel(check.blue, 0.001f));
    REQUIRE_THAT(result.alpha, Catch::WithinRel(check.alpha, 0.001f));
  };

  check(a + b, color(0.7, 0.85, 0.6, 1.0));
  check(a - b, color(-0.3, -0.35, 0.4, 1.0));
  check(a * 2, color(0.4, 0.5, 1.0, 1.0));
  check(2 * a, color(0.4, 0.5, 1.0, 1.0));
}
