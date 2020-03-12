/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include "app.hpp"

void  draw(canvas& cnv)
{
    cnv.rect({ 20, 20, 100, 60 });
    cnv.fill_style(colors::navy_blue);
    cnv.fill_preserve();

    // REQUIRE(cnv.hit_test({ 30, 30 }));

    cnv.stroke_style(colors::antique_white);
    cnv.line_width(2);
    cnv.stroke();

    cnv.round_rect({ 40, 35, 120, 80 }, 8);
    cnv.fill_style(colors::light_sea_green.opacity(0.5));
    cnv.fill_preserve();

}

int main(int argc, const char* argv[])
{
    return run_app(argc, argv);
}

