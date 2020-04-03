/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include "app.hpp"

using namespace cycfi::artist;
constexpr auto xsize = 150.0f;
constexpr auto ysize = 170.0f;
auto constexpr window_size = point{ (xsize*4)*2, (ysize*3)+10 };
auto constexpr bkd_color = colors::white;

void background(canvas& cnv)
{
   cnv.rect({ { 0, 0 }, window_size });
   cnv.fill_style(bkd_color);
   cnv.fill();
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
   };
}

picture dest{ "dest.png" };
picture src{ "src.png" };

void composite_draw(canvas& cnv, point p, canvas::composite_op_enum mode)
{
   {
      auto save = cnv.new_state();
      cnv.rect({ p.x, p.y, p.x+xsize, p.y+ysize });
      cnv.clip();

      cnv.global_composite_operation(canvas::source_over);
      cnv.draw(dest, p, 0.5);
      cnv.global_composite_operation(mode);
      cnv.draw(src, p, 0.5);
   }

   cnv.fill_style(colors::black);
   cnv.text_align(cnv.center);
   cnv.text_baseline(cnv.bottom);
   cnv.fill_text(mode_name(mode), p.x+(xsize/2), p.y+ysize);
}

void composite_ops(canvas& cnv)
{
   cnv.font(font_descr{ "Open Sans", 10 });

   auto col1 = 0;
   auto col2 = xsize*4;

   composite_draw(cnv, { 0, 0 }, cnv.source_over);
   composite_draw(cnv, { xsize, 0 }, cnv.source_atop);
   composite_draw(cnv, { xsize*2, 0 }, cnv.source_in);
   composite_draw(cnv, { xsize*3, 0 }, cnv.source_out);

   composite_draw(cnv, { 0, ysize }, cnv.destination_over);
   composite_draw(cnv, { xsize, ysize }, cnv.destination_atop);
   composite_draw(cnv, { xsize*2, ysize }, cnv.destination_in);
   composite_draw(cnv, { xsize*3, ysize }, cnv.destination_out);

   composite_draw(cnv, { 0, ysize*2 }, cnv.lighter);
   composite_draw(cnv, { xsize, ysize*2 }, cnv.darker);
   composite_draw(cnv, { xsize*2, ysize*2 }, cnv.copy);
   composite_draw(cnv, { xsize*3, ysize*2 }, cnv.xor_);

   composite_draw(cnv, { col2, 0 }, cnv.difference);
   composite_draw(cnv, { col2+xsize, 0 }, cnv.exclusion);
   composite_draw(cnv, { col2+(xsize*2), 0 }, cnv.multiply);
   composite_draw(cnv, { col2+(xsize*3), 0 }, cnv.screen);

   composite_draw(cnv, { col2, ysize }, cnv.color_dodge);
   composite_draw(cnv, { col2+xsize, ysize }, cnv.color_burn);
   composite_draw(cnv, { col2+(xsize*2), ysize }, cnv.soft_light);
   composite_draw(cnv, { col2+(xsize*3), ysize }, cnv.hard_light);

   composite_draw(cnv, { col2, ysize*2 }, cnv.hue);
   composite_draw(cnv, { col2+xsize, ysize*2 }, cnv.saturation);
   composite_draw(cnv, { col2+(xsize*2), ysize*2 }, cnv.color_op);
   composite_draw(cnv, { col2+(xsize*3), ysize*2 }, cnv.luminosity);
}

void draw(canvas& cnv)
{
   // checkered(cnv, { 0, 0 }, 640.0f, 128);
   // composite_draw(cnv);

   // background(cnv);
   composite_ops(cnv);

   // picture pm{ window_size };
   // {
   //     picture_context ctx{pm };
   //     canvas pm_cnv{ ctx.context() };
   //     composite_ops(pm_cnv);
   // }

   // background(cnv);
   // cnv.draw(pm);
}

int main(int argc, const char* argv[])
{
   return run_app(argc, argv, window_size, bkd_color);
}

