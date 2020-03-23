/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include "app.hpp"
#include <map>

using namespace cycfi::artist;
auto constexpr window_size = point{ 640.0f, 480.0f };
auto constexpr bkd_color = rgb(44, 42, 45);

void background(canvas& cnv)
{
    cnv.rect({ { 0, 0 }, window_size });
    cnv.fill_style(bkd_color);
    cnv.fill();
}

void draw(canvas& cnv)
{
    background(cnv);

    cnv.line_width(2);

    cnv.rect(20, 20, 150, 100);
    cnv.fill_style(colors::red);
    cnv.fill();

    cnv.rect(20, 20, 150, 100);
    cnv.stroke_style(colors::navajo_white.opacity(0.5));
    cnv.stroke();

    cnv.round_rect(50, 50, 150, 100, 10);
    cnv.fill_style(colors::blue.opacity(0.5));
    cnv.fill();

    cnv.round_rect(50, 50, 150, 100, 10);
    cnv.stroke_style(colors::honeydew.opacity(0.5));
    cnv.stroke();

    for (int i = 0; i != 7; ++i)
    {
        auto cx = 280+(i*45);
        auto cy = 75;
        auto ri = 7-i;
        auto radius = 20+(ri*5);

        cnv.circle(cx, cy, radius);
        cnv.fill_style(hsl(360/7 * i, 0.8, 0.5).opacity(0.7));
        cnv.fill();

        cnv.line_width((ri+1)/2);
        cnv.stroke_style(colors::light_sky_blue.opacity(0.8));
        cnv.arc(cx, cy, radius, 0.0f, M_PI + (M_PI * ((i+1)/7.0f)), true);
        cnv.stroke();
    }

}

int main(int argc, const char* argv[])
{
    return run_app(argc, argv, window_size);
}

