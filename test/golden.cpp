#include "test_support.hpp"

#include <iostream>

void background(canvas& cnv)
{
   cnv.add_rect({{0, 0}, window_size});
   cnv.fill_style(bkd_color);
   cnv.fill();
}

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

