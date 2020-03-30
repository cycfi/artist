/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include "app.hpp"
#include <map>

/*
using namespace cycfi::artist;
auto constexpr window_size = point{ 640.0f, 640.0f };
auto constexpr bkd_color = colors::white;

void background(canvas& cnv)
{
    cnv.rect({ { 0, 0 }, window_size });
    cnv.fill_style(bkd_color);
    cnv.fill();
}

void checkered(canvas& cnv, point p, float size, int div)
{
    cnv.fill_style(colors::white);
    cnv.rect({ p.x, p.y, p.x+size, p.y+size });
    cnv.fill();

    auto p2 = p;
    auto const box_size = size/div;
    cnv.fill_style(colors::gray[90]);

    for (int y = 0; y < div; ++y)
    {
        for (int x = 0; x < (div/2); ++x)
        {
            cnv.rect({ p2.x, p2.y, p2.x+box_size, p2.y+box_size });
            cnv.fill();
            p2.x += box_size * 2;
        }
        p2.x = p.x + ((y % 2)? 0 : box_size);
        p2.y += box_size;
    }
}

constexpr auto scale = 0.25f;
auto rgb_blur = picture{ "rgb_blur.png" };
auto rgb_circles = picture{ "rgb_circles.png" };
auto pict_size = extent{ rgb_blur.size().x*scale, rgb_blur.size().y*scale };

void draw_images(canvas& cnv, point p, canvas::composite_op_enum mode)
{
    //checkered(cnv, p, pict_size.x);
    cnv.global_composite_operation(cnv.source_over);
    cnv.draw(rgb_blur, p, scale);
    cnv.global_composite_operation(mode);
    cnv.draw(rgb_circles, p, scale);
    cnv.global_composite_operation(cnv.source_over);
}

constexpr canvas::composite_op_enum const modes[] = {
    canvas::source_over,
    canvas::source_in,
    canvas::source_out,
    canvas::source_atop,
    canvas::destination_over,
    canvas::destination_in,
    canvas::destination_out,
    canvas::destination_atop,
    canvas::lighter,
    canvas::darker,
    canvas::copy,
    canvas::xor_
};

void composite_op(canvas& cnv)
{
    int mode = canvas::source_over;
    background(cnv);
    for (int x = 0; x != 4; ++x)
        for (int y = 0; y != 4; ++y)
            draw_images(
                cnv,
                { 20+(x*pict_size.x), 20+(y*pict_size.y) },
                cnv.source_in // modes[mode++]
            );
}

void composite_draw(canvas& cnv)
{
    cnv.fill_style(colors::red);
    cnv.fill_rect({ { 20, 20 }, { 75, 50 } });
    cnv.global_composite_operation(cnv.source_over);
    cnv.fill_style(colors::blue);
    cnv.fill_rect({ { 50, 50 }, { 75, 50} });

    cnv.fill_style(colors::red);
    cnv.fill_rect({ { 150, 20 }, { 75, 50 } });
    cnv.global_composite_operation(cnv.destination_over);
    cnv.fill_style(colors::blue);
    cnv.fill_rect({ { 180, 50 }, { 75, 50 } });
}

void draw(canvas& cnv)
{
    // checkered(cnv, { 0, 0 }, 640.0f, 128);
    // composite_draw(cnv);
    composite_op(cnv);
}
*/

using namespace cycfi::artist;
constexpr auto size = 170;
auto constexpr window_size = point{ size*4, size*3 };
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
    };
}

picture rgb_blur{ "rgb_blur.png" };
picture rgb_circles{ "rgb_circles.png" };

void composite_draw(canvas& cnv, point p, canvas::composite_op_enum mode)
{
    {
        // cnv.stroke_style(colors::blue);
        // cnv.rect(p.x, p.y, size, size);
        // cnv.stroke();

        auto save = cnv.new_state();
        cnv.rect({ p.x, p.y, p.x+size, p.y+size });
        cnv.clip();

        cnv.draw(rgb_blur, p, 0.5);
        cnv.draw(rgb_circles, p, 0.5);

        // cnv.global_composite_operation(cnv.source_over);
        // cnv.fill_style(colors::blue);
        // cnv.fill_rect(p.x+20, p.y+20, 60, 60);
        // cnv.global_composite_operation(mode);
        // cnv.fill_style(colors::red);
        // cnv.circle(p.x+70, p.y+70, 30);
        // cnv.fill();
    }

    cnv.fill_style(colors::black);
    cnv.text_align(cnv.center | cnv.bottom);
    cnv.fill_text(mode_name(mode), p.x+(size/2), p.y+size);
}

void composite_ops(canvas& cnv)
{
    cnv.font(font_descr{ "Open Sans", 10 });


    composite_draw(cnv, { 0, 0 }, cnv.source_over);
    composite_draw(cnv, { size, 0 }, cnv.source_atop);
    composite_draw(cnv, { size*2, 0 }, cnv.source_in);
    composite_draw(cnv, { size*3, 0 }, cnv.source_out);

    composite_draw(cnv, { 0, size }, cnv.destination_over);
    composite_draw(cnv, { size, size }, cnv.destination_atop);
    composite_draw(cnv, { size*2, size }, cnv.destination_in);
    composite_draw(cnv, { size*3, size }, cnv.destination_out);

    composite_draw(cnv, { 0, size*2 }, cnv.lighter);
    composite_draw(cnv, { size, size*2 }, cnv.darker);
    composite_draw(cnv, { size*2, size*2 }, cnv.copy);
    composite_draw(cnv, { size*3, size*2 }, cnv.xor_);
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
    return run_app(argc, argv, window_size, colors::white);
}

