/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include "app.hpp"

using namespace cycfi::artist;
auto constexpr window_size = point{ 640.0f, 480.0f };
auto constexpr bkd_color = rgb(44, 42, 45);

void background(canvas& cnv)
{
   cnv.rect({ { 0, 0 }, window_size });
   cnv.fill_style(bkd_color);
   cnv.fill();
}

void logo(canvas& cnv)
{
   cnv.begin_path();
   cnv.move_to(240.91255, 162.6616);
   cnv.line_to(443.74182, 430);
   cnv.bezier_curve_to(443.74182, 430, 344.88091, 430.11495, 344.88091, 430.11495);
   cnv.bezier_curve_to(344.88091, 430.11495, 356.42931, 419.30399, 362.49098, 412.5);
   cnv.bezier_curve_to(376.86127, 396.36989, 372.87291, 387.78661, 311.54085, 302.85105);
   cnv.bezier_curve_to(274.94338, 252.16912, 242.75, 210.70209, 240, 210.70209);
   cnv.bezier_curve_to(237.25, 210.70209, 205.05662, 252.16912, 168.45915, 302.85105);
   cnv.bezier_curve_to(107.12709, 387.78661, 103.13873, 396.36989, 117.50902, 412.5);
   cnv.bezier_curve_to(123.42655, 419.1422, 132.59451, 429.93021, 132.59451, 429.93021);
   cnv.line_to(36.258176, 430);
   cnv.bezier_curve_to(36.258176, 430, 240.91255, 162.6616, 240.91255, 162.6616);
   cnv.fill();

   cnv.begin_path();
   cnv.line_width(25);
   cnv.circle(240, 80, 40);
   cnv.stroke();
}

void tauri(canvas& cnv)
{
   // Glow
   cnv.fill_style(bkd_color);
   cnv.stroke_style(bkd_color);
   cnv.shadow_style({ -1, -1 }, 10, colors::light_cyan);
   logo(cnv);

   // Shadow
   cnv.fill_style(colors::black);// bkd_color);
   cnv.stroke_style(colors::black); // (bkd_color);
   cnv.shadow_style({ 5.0, 5.0 }, 20, rgb(0, 0, 20));
   logo(cnv);

   // Gradient
   auto gr = canvas::linear_gradient{ 0, 0, 500, 500 };
   gr.add_color_stop(0.0, colors::gold);
   gr.add_color_stop(1.0, colors::gold.opacity(0));

   cnv.fill_style(gr);
   cnv.stroke_style(gr);
   logo(cnv);
}

void draw(canvas& cnv)
{
   cnv.translate(85, 0);
   tauri(cnv);
}

// void draw(canvas& cnv)
// {
//    cnv.scale(12, 12);
//    cnv.line_width(1);
//    cnv.circle({ 20, 20, 16 });
//    cnv.stroke_style(colors::black);
//    cnv.stroke();
// }

int main(int argc, char const* argv[])
{
   return run_app(argc, argv, window_size, bkd_color);
}

