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
   cnv.move_to(24.091255, 16.26616);
   cnv.line_to(44.374182, 43);
   cnv.bezier_curve_to(44.374182, 43, 34.488091, 43.011495, 34.488091, 43.011495);
   cnv.bezier_curve_to(34.488091, 43.011495, 35.642931, 41.930399, 36.249098, 41.25);
   cnv.bezier_curve_to(37.686127, 39.636989, 37.287291, 38.778661, 31.154085, 30.285105);
   cnv.bezier_curve_to(27.494338, 25.216912, 24.275, 21.070209, 24, 21.070209);
   cnv.bezier_curve_to(23.725, 21.070209, 20.505662, 25.216912, 16.845915, 30.285105);
   cnv.bezier_curve_to(10.712709, 38.778661, 10.313873, 39.636989, 11.750902, 41.25);
   cnv.bezier_curve_to(12.342655, 41.91422, 13.259451, 42.993021, 13.259451, 42.993021);
   cnv.line_to(3.6258176,43);
   cnv.bezier_curve_to(3.6258176, 43, 24.091255, 16.26616, 24.091255, 16.26616);
   cnv.fill();

   cnv.begin_path();
   cnv.line_width(2.5);
   cnv.circle(24, 8, 4);
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
   cnv.fill_style(bkd_color);
   cnv.stroke_style(bkd_color);
   cnv.shadow_style({ 5.0, 5.0 }, 20, rgb(0, 0, 20));
   logo(cnv);

   // Gradient
   auto gr = canvas::linear_gradient{ 0, 0, 50, 50 };
   gr.add_color_stop(0.0, colors::gold);
   gr.add_color_stop(1.0, colors::gold.opacity(0));

   cnv.fill_style(gr);
   cnv.stroke_style(gr);
   logo(cnv);
}

void draw(canvas& cnv)
{
   background(cnv);
   cnv.scale(10, 10);
   cnv.translate(9, 0);
   tauri(cnv);
}

int main(int argc, const char* argv[])
{
   return run_app(argc, argv, window_size);
}

