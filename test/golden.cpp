#include "test_support.hpp"

#include <algorithm>
#include <iostream>
#include <infra/filesystem.hpp>

void background(canvas& cnv)
{
   cnv.add_rect({{0, 0}, window_size});
   cnv.fill_style(bkd_color);
   cnv.fill();
}

// The golden comparison is pixelmatch's: a pixel differs when the YIQ
// difference of its colour passes a threshold and it is not an anti-aliased
// edge in either image. Both images are composited over white and over
// black, so a change in alpha shows as a change in colour. No differing
// pixel is allowed: rounding across platforms stays under the threshold,
// and edges that move by a fraction of a pixel are anti-aliased.
namespace
{
   // Half pixelmatch's default of 0.1: the tightest that passes text and
   // blur rendered on other platforms (a flat grey may move about 13 of
   // 255 levels).
   constexpr double match_threshold = 0.05;
   constexpr double max_delta = 35215 * match_threshold * match_threshold;

   struct rgba
   {
      double r, g, b, a;
   };

   // pixels() is premultiplied ARGB32 on every backend.
   rgba unpack(uint32_t px)
   {
      return {
         double((px >> 16) & 0xFF), double((px >> 8) & 0xFF)
       , double(px & 0xFF), double((px >> 24) & 0xFF)
      };
   }

   // Premultiplied, so over a background bg the colour is c + bg * (1 - a).
   rgba over(rgba c, double bg)
   {
      double k = bg * (1 - c.a / 255);
      return {c.r + k, c.g + k, c.b + k, 255};
   }

   double luma(rgba c)
   {
      return c.r * 0.29889531 + c.g * 0.58662247 + c.b * 0.11448223;
   }

   // The squared YIQ distance, weighted as in pixelmatch.
   double delta(rgba c1, rgba c2)
   {
      double dr = c1.r - c2.r, dg = c1.g - c2.g, db = c1.b - c2.b;
      double y = dr * 0.29889531 + dg * 0.58662247 + db * 0.11448223;
      double i = dr * 0.59597799 - dg * 0.27417610 - db * 0.32180189;
      double q = dr * 0.21147017 - dg * 0.52261711 + db * 0.31114694;
      return 0.5053 * y * y + 0.299 * i * i + 0.1957 * q * q;
   }

   struct pixels_view
   {
      uint32_t at(int x, int y) const { return px[y * w + x]; }

      uint32_t const*   px;
      int               w, h;
   };

   // More than two of the 3x3 neighbours (an image edge counts as one)
   // have the same pixel as (x1, y1).
   bool has_many_siblings(pixels_view img, int x1, int y1)
   {
      int x0 = std::max(x1 - 1, 0), x2 = std::min(x1 + 1, img.w - 1);
      int y0 = std::max(y1 - 1, 0), y2 = std::min(y1 + 1, img.h - 1);
      int zeroes = (x1 == x0 || x1 == x2 || y1 == y0 || y1 == y2)? 1 : 0;
      auto c = img.at(x1, y1);
      for (int y = y0; y <= y2; ++y)
         for (int x = x0; x <= x2; ++x)
            if ((x != x1 || y != y1) && img.at(x, y) == c && ++zeroes > 2)
               return true;
      return false;
   }

   // (x1, y1) is an anti-aliased edge pixel of img: it lies between a
   // darker and a brighter neighbour, either of which sits in a flat area
   // in both images.
   bool antialiased(pixels_view img, int x1, int y1, pixels_view other)
   {
      int x0 = std::max(x1 - 1, 0), x2 = std::min(x1 + 1, img.w - 1);
      int y0 = std::max(y1 - 1, 0), y2 = std::min(y1 + 1, img.h - 1);
      int zeroes = (x1 == x0 || x1 == x2 || y1 == y0 || y1 == y2)? 1 : 0;
      double c = luma(over(unpack(img.at(x1, y1)), 255));
      double min = 0, max = 0;
      int min_x = 0, min_y = 0, max_x = 0, max_y = 0;
      for (int y = y0; y <= y2; ++y)
      {
         for (int x = x0; x <= x2; ++x)
         {
            if (x == x1 && y == y1)
               continue;
            double d = c - luma(over(unpack(img.at(x, y)), 255));
            if (d == 0)
            {
               if (++zeroes > 2)
                  return false;
            }
            else if (d < min)
            {
               min = d; min_x = x; min_y = y;
            }
            else if (d > max)
            {
               max = d; max_x = x; max_y = y;
            }
         }
      }
      if (min == 0 || max == 0)
         return false;
      return (has_many_siblings(img, min_x, min_y) && has_many_siblings(other, min_x, min_y))
         || (has_many_siblings(img, max_x, max_y) && has_many_siblings(other, max_x, max_y));
   }

   int count_differences(uint32_t const* a, uint32_t const* b, int w, int h)
   {
      pixels_view va{a, w, h}, vb{b, w, h};
      int n = 0;
      for (int y = 0; y < h; ++y)
      {
         for (int x = 0; x < w; ++x)
         {
            auto i = y * w + x;
            if (a[i] == b[i])
               continue;
            auto pa = unpack(a[i]), pb = unpack(b[i]);
            if (delta(over(pa, 255), over(pb, 255)) <= max_delta
               && delta(over(pa, 0), over(pb, 0)) <= max_delta)
               continue;
            if (antialiased(va, x, y, vb) || antialiased(vb, x, y, va))
               continue;
            ++n;
         }
      }
      return n;
   }
}

#if defined(ARTIST_RECORDING)

#include <fstream>
#include <sstream>

// The recording backend's golden is the journal as text. A missing golden is
// written on the first run; after that the journal must match it line for
// line, and the first line that differs is reported.
static void journal_golden(image const& pm, std::string const& name)
{
   auto text = recording::journal_of(pm).str();
   std::ofstream{get_results_path() + name + ".txt"} << text;

   auto path = get_golden_path() + name + ".txt";
   if (!cycfi::fs::exists(path))
   {
      cycfi::fs::create_directories(get_golden_path());
      std::ofstream{path} << text;
      std::cout << "[golden created] " << name << "\n";
      return;
   }

   std::ifstream f{path};
   std::stringstream golden;
   golden << f.rdbuf();

   std::istringstream a{golden.str()}, b{text};
   std::string la, lb;
   int line = 0;
   while (true)
   {
      bool more_a = bool(std::getline(a, la));
      bool more_b = bool(std::getline(b, lb));
      ++line;
      if (!more_a && !more_b)
         break;
      if (more_a != more_b || la != lb)
      {
         INFO(name << " line " << line << "\n  golden: " << la << "\n  result: " << lb);
         CHECK(false);
         return;
      }
   }
}

void compare_golden(image const& pm, std::string name)
{
   journal_golden(pm, name);
}

void snapshot_golden(image const& pm, std::string name)
{
   journal_golden(pm, name);
}

#else

int golden_differences(image const& a, image const& b)
{
   auto size = a.bitmap_size();
   return count_differences(a.pixels(), b.pixels(), size.x, size.y);
}

void compare_golden(image const& pm, std::string name)
{
   pm.save_png(get_results_path() + name + ".png");
   auto golden = image(get_golden_path() + name + ".png");
   auto result = image(get_results_path() + name + ".png");

   auto bm_size = result.bitmap_size();
   CHECK(bm_size == golden.bitmap_size());
   if (!(bm_size == golden.bitmap_size()))
      return;   // don't compare mismatched buffers

   // The result is a loaded PNG (scale 1), so its size() is the pixel size,
   // which is the logical window scaled by golden_scale.
   auto size = result.size();
   CHECK(size.x == Approx(window_size.x * golden_scale));
   CHECK(size.y == Approx(window_size.y * golden_scale));

   auto a = golden.pixels();
   auto b = result.pixels();

   REQUIRE(a != nullptr);
   REQUIRE(b != nullptr);

   int diff = count_differences(a, b, bm_size.x, bm_size.y);
   std::cout << "differing pixels=" << diff << "  (" << name << ")\n";
   CHECK(diff == 0);
}

void snapshot_golden(image const& pm, std::string name)
{
   // Bootstrap: on the first run no golden exists yet, so save this render as
   // the golden and pass. On subsequent runs, compare against it. (Loading a
   // missing PNG throws on some backends, so check existence first.)
   // Unlike compare_golden this is size-agnostic: the image may be any size
   // (e.g. a tall caret-following editing window, not the 640x480 canvas).
   auto golden_path = get_golden_path() + name + ".png";
   pm.save_png(get_results_path() + name + ".png");
   if (!cycfi::fs::exists(golden_path))
   {
      pm.save_png(golden_path);
      std::cout << "[golden created] " << name << "\n";
      return;
   }

   auto golden = image(golden_path);
   auto result = image(get_results_path() + name + ".png");
   auto gs = golden.bitmap_size();
   auto rs = result.bitmap_size();
   CHECK(rs == gs);
   if (!(rs == gs))
      return;

   auto a = golden.pixels();
   auto b = result.pixels();
   REQUIRE(a != nullptr);
   REQUIRE(b != nullptr);

   int diff = count_differences(a, b, rs.x, rs.y);
   std::cout << "differing pixels=" << diff << "  (" << name << ")\n";
   CHECK(diff == 0);
}

#endif

