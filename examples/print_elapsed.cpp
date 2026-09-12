/*=============================================================================
   Copyright (c) 2016-2023 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include "app.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <numeric>
#include <vector>

using namespace cycfi::artist;

extern float elapsed_;

namespace
{
   // ARTIST_PERF=N: after 60 warmup frames, sample N render times, print one
   // summary line to stdout and exit. This is the artist-side counterpart of
   // ELEMENTS_PERF, so the same examples that show fps on screen can be
   // measured unattended. ARTIST_PERF_LABEL names the run in the line.
   struct perf_recorder
   {
      perf_recorder()
      {
         if (char const* e = std::getenv("ARTIST_PERF"))
         {
            on = true;
            if (int n = std::atoi(e); n > 0)
               frames = n;
         }
      }

      void record(float seconds)
      {
         if (!on)
            return;
         if (seen++ < warmup)
            return;
         samples.push_back(seconds * 1000.0f);
         if (int(samples.size()) < frames)
            return;

         std::sort(samples.begin(), samples.end());
         auto n = samples.size();
         auto median = samples[n / 2];
         auto mean = std::accumulate(samples.begin(), samples.end(), 0.0f) / n;
         auto p95 = samples[std::min(n - 1, std::size_t(n * 0.95))];
         auto min = samples.front();
         char const* label = std::getenv("ARTIST_PERF_LABEL");
         std::printf(
            "ARTIST_PERF example=%s backend=%s samples=%zu warmup=%d "
            "draw_ms{median=%.3f,mean=%.3f,p95=%.3f,min=%.3f} "
            "fps{median=%.1f,mean=%.1f}\n",
            label? label : "?", backend(), n, warmup,
            median, mean, p95, min, 1000.0f / median, 1000.0f / mean);
         std::fflush(stdout);
         std::_Exit(0);
      }

      static char const* backend()
      {
#if defined(ARTIST_SKIA)
         return "skia";
#elif defined(ARTIST_CAIRO)
         return "cairo";
#elif defined(ARTIST_DIRECT2D)
         return "direct2d";
#elif defined(ARTIST_QUARTZ_2D)
         return "quartz2d";
#else
         return "unknown";
#endif
      }

      bool                 on = false;
      int                  warmup = 60;
      int                  frames = 600;
      int                  seen = 0;
      std::vector<float>   samples;
   };

   perf_recorder perf_;
}

template <int n>
struct exp_moving_average
{
   static constexpr float b = 2.0f / (n + 1);
   static constexpr float b_ = 1.0f - b;

   exp_moving_average(float y_ = 0.0f)
      : y(y_)
   {}

   float operator()(float s)
   {
      return y = b * s + b_ * y;
   }

   float y = 0.0f;
};

void print_elapsed(canvas& cnv, point br, color bkd, color c)
{
   static font open_sans = font_descr{"Open Sans", 12};
   static auto metrics = open_sans.metrics();
   static auto height = metrics.ascent + metrics.leading + metrics.descent;
   static exp_moving_average<256> ma;
   static int refresh = 0;
   static std::string fps_str;

   perf_.record(elapsed_);
   auto ave = ma(elapsed_);

   if (++refresh == 30)
   {
      refresh = 0;
      fps_str = std::to_string(1/ave) + " fps";
   }

   auto width = open_sans.measure_text(fps_str);
   cnv.fill_style(bkd);
   cnv.add_rect({br.x - (width + 4), br.y - height, br.x, br.y});
   cnv.fill();

   cnv.fill_style(c);
   cnv.font(open_sans);
   cnv.text_align(cnv.right | cnv.bottom);
   cnv.fill_text(fps_str, {br.x-2, br.y});
}
