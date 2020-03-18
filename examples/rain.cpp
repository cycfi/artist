/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include "app.hpp"
#include <cstdlib>

using namespace cycfi::artist;

///////////////////////////////////////////////////////////////////////////////
// Ported from Rainbow Rain animation:
// https://onaircode.com/awesome-html5-canvas-examples-source-code/
///////////////////////////////////////////////////////////////////////////////

constexpr auto window_size = point{ 640.0f, 480.0f };
constexpr auto w = window_size.x;
constexpr auto h = window_size.y;
constexpr int total = w;
constexpr auto accelleration = 0.05;
constexpr auto size = w/total;
constexpr auto occupation = w/total;
constexpr auto repaint_color = rgb(0, 0, 0).opacity(0.04);
constexpr auto portion = 360.0f/total;

float dots[total];
float dots_vel[total];

float random_size()
{
    return float(std::rand()) / (RAND_MAX);
}

void rain(canvas& cnv)
{
    cnv.fill_style(repaint_color);
    cnv.fill_rect({ 0, 0, window_size });

    for (auto i = 0; i < total; ++i)
    {
        auto current_y = dots[i] - 1;
        dots[i] += dots_vel[i] += accelleration;

        auto dot_size = extent{ size, dots_vel[i] + 1 };
        auto dot_color = hsl(portion * i, 0.8, 0.5);

        // cnv.fill_style(colors::white);

        cnv.fill_style(dot_color);
        cnv.fill_rect({ occupation * i, current_y, dot_size });

        if (dots[i] > h && random_size() < .01)
            dots[i] = dots_vel[i] = 0;
    }
}

picture offscreen{ window_size };

void draw(canvas& cnv)
{
    {
        picture_context ctx{ offscreen };
        canvas rain_cnv{ ctx.context() };
        rain(rain_cnv);
    }
    cnv.draw(offscreen);
}

void init()
{
    for (auto i = 0; i < total; ++i)
    {
        dots[i] = h;
        dots_vel[i] = 10;
    }
}

int main(int argc, const char* argv[])
{
    init();
    return run_app(argc, argv);
}

