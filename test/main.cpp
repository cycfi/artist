/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include "app.hpp"

void basics(canvas& cnv)
{
    cnv.rect({ 20, 20, 100, 60 });
    cnv.fill_style(colors::navy_blue);
    cnv.fill_preserve();

    cnv.stroke_style(colors::antique_white);
    cnv.line_width(2);
    cnv.stroke();

    cnv.round_rect({ 40, 35, 120, 80 }, 8);
    cnv.fill_style(colors::light_sea_green.opacity(0.5));
    cnv.fill();

    cnv.line_width(10);
    cnv.stroke_style(colors::hot_pink);
    cnv.move_to({ 10, 10 });
    cnv.line_to({ 100, 100 });
    cnv.stroke();

    cnv.line_width(4);
    cnv.circle({{ 120, 80 }, 40});
    cnv.fill_style(colors::gold.opacity(0.5));
    cnv.stroke_style(colors::gold);
    cnv.fill_preserve();
    cnv.stroke();

    cnv.line_width(2);
    cnv.begin_path();
    cnv.move_to({ 100, 20 });
    cnv.arc_to({ 150, 20 }, { 150, 70 }, 30);
    cnv.fill_style(colors::blue_violet.opacity(0.4));
    cnv.stroke_style(colors::blue_violet);
    cnv.stroke_preserve();
    cnv.fill();
}

void draw(canvas& cnv)
{
    basics(cnv);

    cnv.scale({ 1.5, 1.5 });
    cnv.translate({ 150, 0 });
    cnv.rotate(0.8);
    basics(cnv);
}

int main(int argc, const char* argv[])
{
    return run_app(argc, argv);
}

