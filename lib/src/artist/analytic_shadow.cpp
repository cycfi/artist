/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <artist/detail/analytic_shadow.hpp>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>

namespace cycfi::artist::detail
{
   namespace
   {
      constexpr double     inf = std::numeric_limits<double>::infinity();
      constexpr double     half_pi = 1.57079632679489661923;
      constexpr double     root_two_pi = 2.50662827463100050242;

      // The blur spreads this many standard deviations past an edge.
      // Beyond it the coverage is within a sixth of a level of 0 or 1.
      constexpr double     spread = 3.25;

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

         for (std::size_t i = 0; i != acc.size(); ++i)
         {
            float const a = std::clamp(acc[i], 0.0f, 1.0f);
            m.alpha[i] = uint8_t(a * 255 + 0.5f);
         }
      }

      struct mask_key
      {
         double            x0, y0, x1, y1, radius, sigma;
         int               w, h;

         bool              operator==(mask_key const&) const = default;
      };

      // The masks already made, most of them corners that many shapes
      // share. The least recently used make way for new ones.
      struct mask_cache
      {
         struct entry
         {
            mask_key       key;
            shadow_mask    mask;
            std::uint64_t  used;
         };

         static constexpr std::size_t  max_entries = 64;
         static constexpr std::size_t  max_bytes = 4 << 20;

         shadow_mask*      find(mask_key const& key);
         shadow_mask&      make(mask_key const& key, std::size_t size);

         std::vector<entry> entries;
         std::size_t       bytes = 0;
         std::uint64_t     clock = 0;
      };

      shadow_mask* mask_cache::find(mask_key const& key)
      {
         for (auto& e : entries)
         {
            if (e.key == key)
            {
               e.used = ++clock;
               return &e.mask;
            }
         }
         return nullptr;
      }

      // A new mask of size bytes, for the caller to fill.
      shadow_mask& mask_cache::make(mask_key const& key, std::size_t size)
      {
         auto full = [&]
         {
            return entries.size() == max_entries || bytes + size > max_bytes;
         };
         auto older = [](entry const& a, entry const& b)
         {
            return a.used < b.used;
         };

         while (!entries.empty() && full())
         {
            auto i = std::min_element(entries.begin(), entries.end(), older);
            bytes -= i->mask.alpha.size();
            if (i != entries.end() - 1)
               *i = std::move(entries.back());
            entries.pop_back();
         }

         bytes += size;
         entries.push_back({key, {}, ++clock});
         auto& m = entries.back().mask;
         m.alpha.assign(size, 0);
         return m;
      }

      // A position on a grid of 1/256 pixel, so that the same shape a
      // hair away finds the same mask.
      double snap(double v)
      {
         return std::round(v * 256) / 256;
      }

      // Cut the axis a..b into an end, a middle and an end, or leave it
      // in one piece where the ends would meet. Returns how many.
      int cut(double a, double b, double r, int reach, int* at)
      {
         int const a0 = int(std::floor(a - reach));
         int const a1 = int(std::ceil(a + r + reach));
         int const b0 = int(std::floor(b - r - reach));
         int const b1 = int(std::ceil(b + reach));

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

   shadow_layout layout_shadow(shadow_shape const& s, double sigma)
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

      l.reach = int(std::ceil(spread * pixel_sigma(sigma)));
      int const nx = cut(s.x0, s.x1, s.radius, l.reach, l.x);
      int const ny = cut(s.y0, s.y1, s.radius, l.reach, l.y);

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
      double const far = key.radius + spread * sg;
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

      auto& m = cache.make(key, std::size_t(w) * h);
      m.width = w;
      m.height = h;
      render({key.x0, key.y0, key.x1, key.y1, key.radius}, sg, m);
      return m;
   }

   int shadow_stops(
      double a, double b, double sigma
    , int from, int to, shadow_stop* stops)
   {
      double const sg = pixel_sigma(sigma);
      double const reach = std::ceil(spread * sg);
      double pos[max_shadow_stops];
      int n = 0;
      pos[n++] = from;
      pos[n++] = to;

      // About each edge the coverage changes: a stop at each pixel center
      // there, or 33 evenly spaced where the blur is wider than that.
      for (double edge : {a, b})
      {
         double const lo = std::max(double(from), std::floor(edge - reach));
         double const hi = std::min(double(to), std::ceil(edge + reach));
         int const pixels = lo < hi? int(hi - lo) : 0;
         if (pixels > 32)
         {
            for (int i = 0; i <= 32; ++i)
               pos[n++] = lo + pixels * (i / 32.0);
         }
         else
         {
            for (int i = 0; i != pixels; ++i)
               pos[n++] = lo + i + 0.5;
         }
      }

      std::sort(pos, pos + n);
      for (int i = 0; i != n; ++i)
      {
         stops[i].offset = float((pos[i] - from) / (to - from));
         stops[i].alpha =
            float(phi((pos[i] - a) / sg) - phi((pos[i] - b) / sg));
      }
      return n;
   }
}
