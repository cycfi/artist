/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]

   Unit test for cycfi::artist::detail's analytic shadows: the blurred
   coverage of a rectangle, rounded rectangle or circle worked out from the
   shape. The oracle is the slow way: the shape drawn at four samples a
   pixel, blurred with a Gaussian and averaged back. Non-graphical.
=============================================================================*/
#include <artist/detail/analytic_shadow.hpp>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <vector>

using namespace cycfi::artist::detail;

static int failures = 0;
#define CHECK(cond) do { if (!(cond)) { \
   std::cerr << "FAIL " << __LINE__ << ": " #cond "\n"; \
   ++failures; } } while (0)

namespace
{
   constexpr int ss = 4;   // oracle samples per pixel, each way

   // How much of the sample column from x to x + step lies in a..b.
   double overlap(double x, double step, double a, double b)
   {
      return std::max(0.0, std::min(x + step, b) - std::max(x, a)) / step;
   }

   // The shape at ss samples per pixel: exact across each row of samples,
   // and averaged over finer rows down each sample.
   std::vector<double> draw(shadow_shape const& s, int w, int h)
   {
      constexpr int fine = 8;
      int const sw = w * ss, sh = h * ss;
      double const step = 1.0 / ss, r = s.radius;
      std::vector<double> img(std::size_t(sw) * sh, 0.0);
      for (int j = 0; j != sh * fine; ++j)
      {
         double const y = (j + 0.5) * step / fine;
         if (y < s.y0 || y > s.y1)
            continue;

         // How far the rounded ends pull the row in from the sides.
         double const up = std::max({s.y0 + r - y, y - (s.y1 - r), 0.0});
         double const in = r - std::sqrt(std::max(0.0, r * r - up * up));
         double* row = img.data() + std::size_t(j / fine) * sw;
         for (int i = 0; i != sw; ++i)
            row[i] += overlap(i * step, step, s.x0 + in, s.x1 - in) / fine;
      }
      return img;
   }

   void blur_rows(std::vector<double>& img, int w, int h, double sigma)
   {
      int const reach = int(std::ceil(5 * sigma));
      std::vector<double> k(2 * reach + 1), row(w);
      double sum = 0;
      for (int i = -reach; i <= reach; ++i)
         sum += k[i + reach] = std::exp(-0.5 * i * i / (sigma * sigma));

      for (int y = 0; y != h; ++y)
      {
         double* p = img.data() + std::size_t(y) * w;
         for (int x = 0; x != w; ++x)
         {
            double acc = 0;
            for (int i = std::max(-reach, -x); i <= reach && x + i < w; ++i)
               acc += k[i + reach] * p[x + i];
            row[x] = acc / sum;
         }
         std::copy(row.begin(), row.end(), p);
      }
   }

   std::vector<double> transposed(
      std::vector<double> const& img, int w, int h)
   {
      std::vector<double> out(img.size());
      for (int y = 0; y != h; ++y)
         for (int x = 0; x != w; ++x)
            out[std::size_t(x) * h + y] = img[std::size_t(y) * w + x];
      return out;
   }

   // The blurred coverage of s over the pixels 0..w by 0..h, the slow way.
   std::vector<double> oracle(
      shadow_shape const& s, double sigma, int w, int h)
   {
      int const sw = w * ss, sh = h * ss;
      auto img = draw(s, w, h);
      blur_rows(img, sw, sh, sigma * ss);
      img = transposed(img, sw, sh);
      blur_rows(img, sh, sw, sigma * ss);
      img = transposed(img, sh, sw);

      std::vector<double> out(std::size_t(w) * h, 0.0);
      for (int j = 0; j != sh; ++j)
         for (int i = 0; i != sw; ++i)
            out[std::size_t(j / ss) * w + i / ss] +=
               img[std::size_t(j) * sw + i] / (ss * ss);
      return out;
   }

   // The largest difference, in levels of 255, between the mask of s over
   // 0..w by 0..h and the oracle.
   double mask_error(shadow_shape const& s, double sigma, int w, int h)
   {
      auto const ref = oracle(s, sigma, w, h);
      auto const& m = shadow_mask_of(s, sigma, 0, 0, w, h);
      double worst = 0;
      for (int y = 0; y != h; ++y)
         for (int x = 0; x != w; ++x)
            worst = std::max(worst, std::abs(
               m.alpha[std::size_t(y) * w + x]
               - 255 * ref[std::size_t(y) * w + x]));
      return worst;
   }

   // A gradient's alpha at t along it, between its stops.
   double stop_alpha(shadow_stop const* stops, int n, double t)
   {
      for (int i = 1; i != n; ++i)
      {
         if (t <= stops[i].offset)
         {
            auto const& a = stops[i - 1];
            auto const& b = stops[i];
            double const span = b.offset - a.offset;
            double const f = span > 0? (t - a.offset) / span : 1;
            return a.alpha + f * (b.alpha - a.alpha);
         }
      }
      return stops[n - 1].alpha;
   }

   // The shadow of s as a backend puts it together from the pieces of
   // its layout, over the pixels x[0].. by y[0]..
   std::vector<double> assemble(
      shadow_shape const& s, double sigma, shadow_layout const& l)
   {
      int const x0 = l.x[0], y0 = l.y[0];
      int const w = l.x[l.nx] - x0, h = l.y[l.ny] - y0;
      std::vector<double> out(std::size_t(w) * h, -1.0);
      shadow_stop stops[max_shadow_stops];

      for (int j = 0; j != l.ny; ++j)
      {
         for (int i = 0; i != l.nx; ++i)
         {
            bool const mid_x = l.nx == 3 && i == 1;
            bool const mid_y = l.ny == 3 && j == 1;
            int const px = l.x[i], py = l.y[j];
            int const pw = l.x[i + 1] - px, ph = l.y[j + 1] - py;
            if (pw <= 0 || ph <= 0)
               continue;

            shadow_mask const* m = nullptr;
            int n = 0;
            if (mid_x && !mid_y)
               n = shadow_stops(s.y0, s.y1, sigma, py, py + ph, stops);
            else if (mid_y && !mid_x)
               n = shadow_stops(s.x0, s.x1, sigma, px, px + pw, stops);
            else if (!mid_x && !mid_y)
               m = &shadow_mask_of(s, sigma, px, py, pw, ph);

            for (int y = 0; y != ph; ++y)
            {
               for (int x = 0; x != pw; ++x)
               {
                  double a = 255;
                  if (m)
                     a = m->alpha[std::size_t(y) * pw + x];
                  else if (n)
                     a = 255 * stop_alpha(
                        stops, n, mid_x? (y + 0.5) / ph : (x + 0.5) / pw);
                  out[std::size_t(py - y0 + y) * w + (px - x0 + x)] = a;
               }
            }
         }
      }
      return out;
   }

   // The largest difference, in levels, between the pieces put together
   // and the oracle.
   double pieces_error(
      shadow_shape const& s, double sigma, shadow_layout const& l)
   {
      int const x0 = l.x[0], y0 = l.y[0];
      int const w = l.x[l.nx] - x0, h = l.y[l.ny] - y0;
      auto const got = assemble(s, sigma, l);
      shadow_shape const moved{
         s.x0 - x0, s.y0 - y0, s.x1 - x0, s.y1 - y0, s.radius};
      auto const ref = oracle(moved, sigma, w, h);
      double worst = 0;
      for (std::size_t i = 0; i != got.size(); ++i)
         worst = std::max(worst, std::abs(got[i] - 255 * ref[i]));
      return worst;
   }
}

int main()
{
   // A mask is the Gaussian blur of the shape, to within a level or so:
   // a rectangle off the pixel grid, a rounded one, a circle, one smaller
   // than its blur, and a blur of under a pixel.
   CHECK(mask_error({10.25, 12.5, 50.75, 40.0, 0}, 3, 64, 56) < 1.0);
   CHECK(mask_error({10.25, 12.5, 50.75, 40.0, 8}, 3, 64, 56) < 1.0);
   CHECK(mask_error({14.5, 14.5, 44.5, 44.5, 15}, 2, 60, 60) < 1.0);
   CHECK(mask_error({24, 24, 32, 30, 2}, 6, 56, 54) < 1.0);
   CHECK(mask_error({8.5, 8.25, 40, 30, 6}, 0.6, 48, 40) < 1.5);

   // A shape with room for its blur is cut in three each way. Its pieces
   // tile the shadow, and put together they are that same blur: the
   // corner masks, the gradients along the sides and the plain middle
   // meet without a seam.
   {
      shadow_shape const s{20.25, 30.5, 130.5, 110.75, 10};
      auto const l = layout_shadow(s, 3);
      CHECK(l.nx == 3 && l.ny == 3);
      CHECK(l.x[0] < l.x[1] && l.x[1] <= l.x[2] && l.x[2] < l.x[3]);
      CHECK(l.y[0] < l.y[1] && l.y[1] <= l.y[2] && l.y[2] < l.y[3]);
      CHECK(l.x[0] <= s.x0 - l.reach && l.x[3] >= s.x1 + l.reach);
      CHECK(l.reach >= 9);
      CHECK(pieces_error(s, 3, l) < 1.5);
   }

   // A plain rectangle the same.
   {
      shadow_shape const s{16, 16, 96.5, 80.25, 0};
      auto const l = layout_shadow(s, 4);
      CHECK(l.nx == 3 && l.ny == 3);
      CHECK(pieces_error(s, 4, l) < 1.5);
   }

   // A pill has no middle down its short side: the ends are masks, with
   // one gradient down between them.
   {
      shadow_shape const s{20, 20, 140, 40, 10};
      auto const l = layout_shadow(s, 3);
      CHECK(l.nx == 3 && l.ny == 1);
      CHECK(pieces_error(s, 3, l) < 1.5);
   }

   // A circle, and anything not larger than its blur, is one mask.
   {
      shadow_shape const s{30, 30, 70, 70, 20};
      auto const l = layout_shadow(s, 3);
      CHECK(l.nx == 1 && l.ny == 1);
      CHECK(pieces_error(s, 3, l) < 1.5);

      auto const small = layout_shadow({30, 30, 50, 42, 0}, 8);
      CHECK(small.nx == 1 && small.ny == 1);
   }

   // A corner's mask does not depend on the size of the shape, or on
   // where it is by whole pixels: two shapes share it.
   {
      shadow_shape const a{20.5, 30.25, 130, 110, 6};
      shadow_shape const b{57.5, 91.25, 400, 300, 6};
      auto const la = layout_shadow(a, 2), lb = layout_shadow(b, 2);
      int const w = la.x[1] - la.x[0], h = la.y[1] - la.y[0];
      CHECK(w == lb.x[1] - lb.x[0] && h == lb.y[1] - lb.y[0]);

      auto const first = shadow_mask_of(a, 2, la.x[0], la.y[0], w, h).alpha;
      auto const& second = shadow_mask_of(b, 2, lb.x[0], lb.y[0], w, h);
      CHECK(first == second.alpha);
      CHECK(second.width == w && second.height == h);
   }

   // Nothing to lay out: an empty shape, no blur, and a blur so wide
   // that its masks would be larger than blurring pixels is worth.
   CHECK(layout_shadow({10, 10, 10, 40, 0}, 3).nx == 0);
   CHECK(layout_shadow({10, 10, 40, 40, 0}, 0).nx == 0);
   CHECK(layout_shadow({0, 0, 4000, 4000, 0}, 2000).nx == 0);

   if (failures == 0)
      std::cout << "analytic_shadow_test: all passed\n";
   return failures != 0;
}
