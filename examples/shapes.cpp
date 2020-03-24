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

void rectangles(canvas& cnv)
{
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
}

void circles_and_arcs(canvas& cnv)
{
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

void arc_to(canvas& cnv)
{
    constexpr auto x = 20;
    constexpr auto y = 180;
    constexpr auto r = 40;

    cnv.stroke_style(colors::dodger_blue.opacity(0.6));
    cnv.line_width(10);
    cnv.begin_path();
    cnv.move_to(x, y);
    cnv.line_to(x+(80-r), y);
    cnv.arc_to(x+80, y, x+80, y+20, r);
    cnv.line_to(x+80, y+80);
    cnv.stroke();

    cnv.stroke_style(colors::white);
    cnv.line_width(1);
    cnv.begin_path();
    cnv.move_to(x, y);
    cnv.line_to(x+80, y);
    cnv.line_to(x+80, y+80);
    cnv.stroke();
}

void line_caps(canvas& cnv)
{
    auto where = point{ 140, 195 };
    auto spacing = 25;
    cnv.line_width(10);

    cnv.stroke_style(colors::gold);
    cnv.begin_path();
    cnv.line_width(10);
    cnv.line_cap(cnv.butt);
    cnv.move_to(where.x, where.y);
    cnv.line_to(where.x+100, where.y);
    cnv.stroke();

    cnv.stroke_style(colors::sky_blue);
    cnv.begin_path();
    cnv.line_cap(cnv.round);
    cnv.move_to(where.x, where.y+spacing);
    cnv.line_to(where.x+100, where.y+spacing);
    cnv.stroke();

    cnv.stroke_style(colors::light_sea_green);
    cnv.begin_path();
    cnv.line_cap(cnv.square);
    cnv.move_to(where.x, where.y+spacing*2);
    cnv.line_to(where.x+100, where.y+spacing*2);
    cnv.stroke();

    cnv.stroke_style(colors::white);
    cnv.line_width(1);

    cnv.begin_path();
    cnv.move_to(where.x, where.y);
    cnv.line_to(where.x+100, where.y);
    cnv.stroke();

    cnv.begin_path();
    cnv.move_to(where.x, where.y+spacing);
    cnv.line_to(where.x+100, where.y+spacing);
    cnv.stroke();

    cnv.begin_path();
    cnv.move_to(where.x, where.y+spacing*2);
    cnv.line_to(where.x+100, where.y+spacing*2);
    cnv.stroke();
}

void line_joins(canvas& cnv)
{
    auto where = point{ 270, 80 };
    cnv.line_width(10);
    cnv.line_cap(cnv.butt);

    cnv.stroke_style(colors::gold);
    cnv.begin_path();
    cnv.line_join(cnv.bevel_join);
    cnv.move_to(where.x, where.y+100);
    cnv.line_to(where.x+60, where.y+140);
    cnv.line_to(where.x, where.y+180);
    cnv.stroke();

    cnv.stroke_style(colors::sky_blue);
    cnv.begin_path();
    cnv.line_join(cnv.round_join);
    cnv.move_to(where.x+40, where.y+100);
    cnv.line_to(where.x+100, where.y+140);
    cnv.line_to(where.x+40, where.y+180);
    cnv.stroke();

    cnv.stroke_style(colors::light_sea_green);
    cnv.begin_path();
    cnv.line_join(cnv.miter_join);
    cnv.move_to(where.x+80, where.y+100);
    cnv.line_to(where.x+140, where.y+140);
    cnv.line_to(where.x+80, where.y+180);
    cnv.stroke();

    cnv.stroke_style(colors::white);
    cnv.line_width(1);

    cnv.begin_path();
    cnv.move_to(where.x, where.y+100);
    cnv.line_to(where.x+60, where.y+140);
    cnv.line_to(where.x, where.y+180);
    cnv.stroke();

    cnv.begin_path();
    cnv.move_to(where.x+40, where.y+100);
    cnv.line_to(where.x+100, where.y+140);
    cnv.line_to(where.x+40, where.y+180);
    cnv.stroke();

    cnv.begin_path();
    cnv.move_to(where.x+80, where.y+100);
    cnv.line_to(where.x+140, where.y+140);
    cnv.line_to(where.x+80, where.y+180);
    cnv.stroke();
}

void beziers(canvas& cnv)
{
    auto where = point{ 450, 260 };
    auto height = 100;
    auto width = 120;
    auto xmove = 20;
    auto start = point{ where.x, where.y };
    auto cp1 = point{ where.x+xmove, where.y-height };
    auto cp2 = point{ where.x+width-xmove, where.y-height };
    auto end = point{ where.x+width, where.y };

    cnv.line_width(10);
    cnv.stroke_style(colors::dodger_blue.opacity(0.6));
    cnv.line_cap(cnv.round);

    cnv.begin_path();
    cnv.move_to(start);
    cnv.bezier_curve_to(cp1, cp2, end);
    cnv.stroke();

    cnv.stroke_style(colors::pink.opacity(0.2));
    cnv.circle(cp1.x, cp1.y, 3);
    cnv.circle(cp2.x, cp2.y, 3);
    cnv.fill();

    cnv.stroke_style(colors::white);
    cnv.line_width(1);
    cnv.move_to(start);
    cnv.line_to(cp1);
    cnv.move_to(cp2);
    cnv.line_to(end);
    cnv.stroke();
}

void draw(canvas& cnv)
{
    background(cnv);
    rectangles(cnv);
    circles_and_arcs(cnv);
    arc_to(cnv);
    line_caps(cnv);
    line_joins(cnv);
    beziers(cnv);
}

int main(int argc, const char* argv[])
{
    return run_app(argc, argv, window_size);
}

