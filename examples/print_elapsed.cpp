/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include "app.hpp"

using namespace cycfi::artist;

extern float elapsed_;

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
   static font open_sans = font_descr{ "Open Sans", 12 };
   static auto metrics = open_sans.metrics();
   static auto height = metrics.ascent + metrics.leading + metrics.descent;
   static exp_moving_average<256> ma;
   static int refresh = 0;
   static std::string fps_str;

   auto ave = ma(elapsed_);

   if (++refresh == 30)
   {
      refresh = 0;
      fps_str = std::to_string(1/ave) + " fps";
   }

   auto width = open_sans.measure_text(fps_str);
   cnv.fill_style(bkd);
   cnv.add_rect({ br.x - (width + 4), br.y - height, br.x, br.y });
   cnv.fill();

   cnv.fill_style(c);
   cnv.font(open_sans);
   cnv.text_align(cnv.right | cnv.bottom);
   cnv.fill_text(fps_str, { br.x-2, br.y });
}
