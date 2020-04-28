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

void print_elapsed(canvas& cnv, point br, color c)
{
   static font open_sans = font_descr{ "Open Sans", 12 };
   static exp_moving_average<256> ma;
   static int refresh = 0;
   static std::string fps_str;

   auto ave = ma(elapsed_);

   if (++refresh == 30)
   {
      refresh = 0;
      fps_str = std::to_string(1/ave) + " fps";
   }

   cnv.fill_style(c);
   cnv.font(open_sans);
   cnv.text_align(cnv.right | cnv.bottom);
   cnv.fill_text(fps_str, { br.x, br.y });
}
