/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <artist/detail/analytic_shadow.hpp>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <unordered_map>

namespace cycfi::artist::detail
{
   namespace
   {
      constexpr double     inf = std::numeric_limits<double>::infinity();
      constexpr double     half_pi = 1.57079632679489661923;
      constexpr double     root_two_pi = 2.50662827463100050242;

      // A side this many standard deviations from a mask, past its
      // rounded ends, has no part in it.
      constexpr double     far_spread = 3.25;

      // A mask larger than this is not made: blurring pixels is no slower.
      constexpr std::int64_t  max_mask_pixels = 1 << 18;

      // A pixel holds the average of the blur over its area: the blur
      // once more by a box a pixel wide, which is close to a Gaussian
      // again with the variances added.
      double pixel_sigma(double sigma)
      {
         return std::sqrt(sigma * sigma + 1.0 / 12);
      }

      // The blurred coverage t standard deviations inside a straight
      // edge: the normal distribution function.
      double phi(double t)
      {
         return 0.5 * std::erfc(-t * 0.70710678118654752440);
      }

      // v as an index into 0..n. Whatever lies outside, the infinities
      // too, lands on an end.
      int index(double v, int n)
      {
         if (!(v > 0))
            return 0;
         return v < n? int(v) : n;
      }

      // Add to out[0..n) k times the coverage of a blurred edge at pos:
      // 0 well before it and k well after, with phi called only between.
      void add_edge(float* out, int n, double pos, double sg, float k)
      {
         int const a = index(std::floor(pos - 4 * sg), n);
         int const b = index(std::ceil(pos + 4 * sg), n);

         for (int i = a; i < b; ++i)
            out[i] += k * float(phi((i + 0.5 - pos) / sg));
         for (int i = b; i < n; ++i)
            out[i] += k;
      }

      // The coverage of the blurred slab a..b at the pixels 0..n.
      void slab(float* out, int n, double a, double b, double sg)
      {
         std::fill(out, out + n, 0.0f);
         add_edge(out, n, a, sg, 1);
         add_edge(out, n, b, sg, -1);
      }

      // Add col to the rows of acc about yc, weighted by a Gaussian of
      // their distance from it.
      void add_rows(
         float* acc, int w, int h
       , float const* col, double yc, double weight, double sg)
      {
         if (!std::isfinite(yc))
            return;
         int const a = index(std::floor(yc - 4 * sg), h);
         int const b = index(std::ceil(yc + 4 * sg), h);

         for (int y = a; y < b; ++y)
         {
            double const t = (y + 0.5 - yc) / sg;
            auto const k = float(weight * std::exp(-0.5 * t * t));
            float* row = acc + std::size_t(y) * w;
            for (int x = 0; x != w; ++x)
               row[x] += k * col[x];
         }
      }

      // The rounded ends, cut into thin slices along the arcs. A slice is
      // a slab across, blurred across in closed form and then down into
      // the rows it reaches. Two Gauss-Legendre points to each sigma of
      // arc keep the sum within a tenth of a level.
      void add_arcs(
         shadow_shape const& s, double sg
       , int w, int h, float* col, float* acc)
      {
         static constexpr double node[] =
            {0.21132486540518712, 0.78867513459481288};

         double const r = s.radius;
         int const panels = std::max(2, int(std::ceil(half_pi * r / sg)));
         double const step = half_pi / panels;
         double const norm = step / 2 * r / (sg * root_two_pi);

         for (int i = 0; i != panels; ++i)
         {
            for (double f : node)
            {
               double const th = (i + f) * step;
               double const dx = r * (1 - std::sin(th));
               double const dy = r * (1 - std::cos(th));
               double const weight = norm * std::sin(th);

               slab(col, w, s.x0 + dx, s.x1 - dx, sg);
               add_rows(acc, w, h, col, s.y0 + dy, weight, sg);
               add_rows(acc, w, h, col, s.y1 - dy, weight, sg);
            }
         }
      }

      // The blurred coverage of s over the mask's pixels, s being placed
      // relative to the mask.
      void render(shadow_shape const& s, double sg, shadow_mask& m)
      {
         thread_local std::vector<float> acc, col, band;
         int const w = m.width, h = m.height;
         acc.resize(std::size_t(w) * h);
         col.resize(w);
         band.resize(h);

         // Between the rounded ends: a slab across times a slab down.
         slab(col.data(), w, s.x0, s.x1, sg);
         slab(band.data(), h, s.y0 + s.radius, s.y1 - s.radius, sg);
         for (int y = 0; y != h; ++y)
         {
            float* row = acc.data() + std::size_t(y) * w;
            for (int x = 0; x != w; ++x)
               row[x] = band[y] * col[x];
         }

         if (s.radius > 0)
            add_arcs(s, sg, w, h, col.data(), acc.data());

         for (int y = 0; y != h; ++y)
         {
            float const* in = acc.data() + std::size_t(y) * w;
            auto* out = m.alpha.data() + std::size_t(y) * m.stride;
            for (int x = 0; x != w; ++x)
               out[x] = uint8_t(std::clamp(in[x], 0.0f, 1.0f) * 255 + 0.5f);
         }
      }

      struct mask_key
      {
         double            x0, y0, x1, y1, radius, sigma;
         int               w, h;

         bool              operator==(mask_key const&) const = default;
      };

      struct key_hash
      {
         std::size_t operator()(mask_key const& k) const noexcept
         {
            std::size_t h = 14695981039346656037ull;
            for (double v : {k.x0, k.y0, k.x1, k.y1, k.radius, k.sigma
               , double(k.w), double(k.h)})
               h = (h ^ std::hash<double>{}(v)) * 1099511628211ull;
            return h;
         }
      };

      // The masks already made, most of them corners and sides that many
      // shapes share. The least recently used make way for new ones.
      struct mask_cache
      {
         struct entry
         {
            shadow_mask    mask;
            std::uint64_t  used = 0;
         };

         using map_type = std::unordered_map<mask_key, entry, key_hash>;

         static constexpr std::size_t  max_entries = 256;
         static constexpr std::size_t  max_bytes = 4 << 20;

         shadow_mask*      find(mask_key const& key);
         shadow_mask&      make(mask_key const& key, std::size_t size);

         map_type          map;
         std::size_t       bytes = 0;
         std::uint64_t     clock = 0;
      };

      shadow_mask* mask_cache::find(mask_key const& key)
      {
         auto const i = map.find(key);
         if (i == map.end())
            return nullptr;
         i->second.used = ++clock;
         return &i->second.mask;
      }

      // A new mask of size bytes, for the caller to fill.
      shadow_mask& mask_cache::make(mask_key const& key, std::size_t size)
      {
         auto full = [&]
         {
            return map.size() >= max_entries || bytes + size > max_bytes;
         };
         auto older = [](auto const& a, auto const& b)
         {
            return a.second.used < b.second.used;
         };

         while (!map.empty() && full())
         {
            auto const i = std::min_element(map.begin(), map.end(), older);
            bytes -= i->second.mask.alpha.size();
            map.erase(i);
         }

         bytes += size;
         auto& e = map[key];
         e.used = ++clock;
         e.mask.alpha.assign(size, 0);
         return e.mask;
      }

      // A position on a grid of 1/256 pixel, so that the same shape a
      // hair away finds the same mask.
      double snap(double v)
      {
         return std::round(v * 256) / 256;
      }

      // How many standard deviations from an edge its blur is down to a
      // coverage of p: the inverse of the normal distribution function,
      // to 0.0005 (Abramowitz and Stegun 26.2.23).
      double spread_to(double p)
      {
         if (p >= 0.5)
            return 0;
         double const t = std::sqrt(-2 * std::log(p));
         return t - (2.515517 + t * (0.802853 + t * 0.010328))
            / (1 + t * (1.432788 + t * (0.189269 + t * 0.001308)));
      }

      // Cut the axis a..b into an end, a middle and an end, or leave it
      // in one piece where the ends would meet. The blur shows as far as
      // out past the shape and in into it. Returns how many pieces.
      int cut(double a, double b, double r, int out, int in, int* at)
      {
         int const a0 = int(std::floor(a - out));
         int const a1 = int(std::ceil(a + r + in));
         int const b0 = int(std::floor(b - r - in));
         int const b1 = int(std::ceil(b + out));

         if (a1 > b0)
         {
            at[0] = a0;
            at[1] = b1;
            return 1;
         }
         at[0] = a0;
         at[1] = a1;
         at[2] = b0;
         at[3] = b1;
         return 3;
      }
   }

   shadow_layout layout_shadow(
      shadow_shape const& s, double sigma, double strength)
   {
      shadow_layout l;
      constexpr double limit = 1 << 24;   // of what int arithmetic takes
      auto in_range = [](double v) { return std::abs(v) < limit; };
      bool const ok =
         s.x0 < s.x1 && s.y0 < s.y1 && s.radius >= 0 && sigma > 0
         && in_range(s.x0) && in_range(s.x1) && in_range(s.y0)
         && in_range(s.y1) && in_range(s.radius) && in_range(sigma);
      if (!ok)
         return l;

      // The shadow is laid out as far as it shows: out to where it would
      // round to nothing at its strength, and in to where a corner, with
      // two edges to fall short by, would round to its full strength.
      double const sg = pixel_sigma(sigma);
      double const level = 1 / (255 * std::clamp(strength, 1.0 / 255, 1.0));
      int const out = int(std::ceil(spread_to(0.5 * level) * sg));
      int const in = int(std::ceil(spread_to(0.25 * level) * sg));
      int const nx = cut(s.x0, s.x1, s.radius, out, in, l.x);
      int const ny = cut(s.y0, s.y1, s.radius, out, in, l.y);

      // The masks are the ends of one axis by the ends of the other.
      auto end = [](int n, int const* at)
      {
         int const last = n == 3? at[3] - at[2] : 0;
         return std::int64_t(std::max(at[1] - at[0], last));
      };
      if (end(nx, l.x) * end(ny, l.y) <= max_mask_pixels)
      {
         l.nx = nx;
         l.ny = ny;
      }
      return l;
   }

   shadow_mask const& shadow_mask_of(
      shadow_shape const& s, double sigma, int x, int y, int w, int h)
   {
      thread_local mask_cache cache;
      double const sg = pixel_sigma(sigma);

      // The shape relative to the mask. A side too far off to reach the
      // mask is as good as one at infinity, so that a corner's mask does
      // not depend on the size of the shape.
      mask_key key{
         snap(s.x0 - x), snap(s.y0 - y), snap(s.x1 - x), snap(s.y1 - y)
       , snap(s.radius), sigma, w, h
      };
      double const far = key.radius + far_spread * sg;
      if (key.x0 + far <= 0)
         key.x0 = -inf;
      if (key.x1 - far >= w)
         key.x1 = inf;
      if (key.y0 + far <= 0)
         key.y0 = -inf;
      if (key.y1 - far >= h)
         key.y1 = inf;

      if (auto* m = cache.find(key))
         return *m;

      int const stride = (w + 3) & ~3;
      auto& m = cache.make(key, std::size_t(stride) * h);
      m.width = w;
      m.height = h;
      m.stride = stride;
      render({key.x0, key.y0, key.x1, key.y1, key.radius}, sg, m);
      return m;
   }
}
