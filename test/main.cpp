/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#define CATCH_CONFIG_MAIN
#include <infra/catch.hpp>
#include "paths.hpp"

using namespace cycfi::artist;

auto constexpr window_size = point{ 640.0f, 480.0f };
auto constexpr bkd_color = rgba(54, 52, 55, 255);

void background(canvas& cnv)
{
    cnv.rect({ { 0, 0 }, window_size });
    cnv.fill_style(bkd_color);
    cnv.fill();
}

void balloon(canvas& cnv)
{
    // quadratic_curve_to
    cnv.begin_path();
    cnv.line_join(cnv.round_join);
    cnv.move_to(75, 25);
    cnv.quadratic_curve_to({ 25, 25 }, { 25, 62.5 });
    cnv.quadratic_curve_to({ 25, 100 }, { 50, 100 });
    cnv.quadratic_curve_to({ 50, 120 }, { 30, 125 });
    cnv.quadratic_curve_to({ 60, 120 }, { 65, 100 });
    cnv.quadratic_curve_to({ 125, 100 }, { 125, 62.5 });
    cnv.quadratic_curve_to({ 125, 25 }, { 75, 25 });
}

void heart(canvas& cnv)
{
    // bezier_curve_to
    constexpr float y0 = 40, y1 = 50, y2 = 23, y3 = 90, y4 = 45, y5 = 75, y6 = 125;
    cnv.begin_path();
    cnv.move_to({ 75, y0 });
    cnv.bezier_curve_to({ 75, y4 }, { 70, y2 }, { 50, y2 });
    cnv.bezier_curve_to({ 20, y2 }, { 20, y1 }, { 20, y1 });
    cnv.bezier_curve_to({ 20, y5 }, { y0, y3 }, { 75, y6 });
    cnv.bezier_curve_to({ 110, y3 }, { 130, y5 }, { 130, y1 });
    cnv.bezier_curve_to({ 130, y1}, { 130, y2 }, { 100, y2 });
    cnv.bezier_curve_to({ 85, y2 }, { 75, y4 }, { 75, y0 });
    cnv.fill_style(color{ 0.2, 0, 0 }.opacity(0.4));
}

void basics(canvas& cnv)
{
    auto state = cnv.new_state();

    cnv.rect(20, 20, 80, 40);
    cnv.fill_style(colors::navy_blue);
    cnv.fill_preserve();

    cnv.stroke_style(colors::antique_white.opacity(0.8));
    cnv.line_width(0.5);
    cnv.stroke();

    cnv.round_rect(40, 35, 80, 45, 8);
    cnv.fill_style(colors::light_sea_green.opacity(0.5));
    cnv.fill();

    cnv.move_to(10, 10);
    cnv.line_to(100, 100);
    cnv.line_width(10);
    cnv.stroke_style(colors::hot_pink);
    cnv.stroke();

    cnv.circle(120, 80, 40);
    cnv.line_width(4);
    cnv.fill_style(colors::gold.opacity(0.5));
    cnv.stroke_style(colors::gold);
    cnv.fill_preserve();
    cnv.stroke();

    cnv.translate(120, 0);
    balloon(cnv);
    cnv.stroke_style(colors::light_gray);
    cnv.stroke();

    cnv.translate(-100, 100);
    heart(cnv);
    cnv.line_width(2);
    cnv.stroke_style(color{ 0.8, 0, 0 });
    cnv.stroke_preserve();
    cnv.fill();
}

void transformed(canvas& cnv)
{
    auto state = cnv.new_state();
    cnv.scale(1.5, 1.5);
    cnv.translate(150, 80);
    cnv.rotate(0.8);
    basics(cnv);
}

void rainbow(canvas::gradient& gr)
{
    gr.add_color_stop(0.0/6, colors::red);
    gr.add_color_stop(1.0/6, colors::orange);
    gr.add_color_stop(2.0/6, colors::yellow);
    gr.add_color_stop(3.0/6, colors::green);
    gr.add_color_stop(4.0/6, colors::blue);
    gr.add_color_stop(5.0/6, rgb(0x4B, 0x00, 0x82));
    gr.add_color_stop(6.0/6, colors::violet);
}

void linear_gradient(canvas& cnv)
{
    auto x = 300.0f;
    auto y = 20.0f;
    auto gr = canvas::linear_gradient{ x, y, x+300, y };
    rainbow(gr);

    cnv.round_rect(x, y, 300, 80, 5);
    cnv.fill_style(gr);
    cnv.fill();
}

void radial_gradient(canvas& cnv)
{
    auto center = point{ 475, 90 };
    auto radius = 75.0f;
    auto gr = canvas::radial_gradient{ center, 5, center.move(15, 10), radius };
    gr.add_color_stop(0.0, colors::red);
    gr.add_color_stop(1.0, colors::black);

    cnv.circle({ center.move(15, 10), radius-10 });
    cnv.fill_style(gr);
    cnv.fill();
}

void stroke_gradient(canvas& cnv)
{
    auto x = 300.0f;
    auto y = 20.0f;
    auto gr = canvas::linear_gradient{ x, y, x+300, y+80 };
    gr.add_color_stop(0.0, colors::navy_blue);
    gr.add_color_stop(1.0, colors::maroon);

    cnv.round_rect(x, y, 300, 80, 5);
    cnv.line_width(8);
    cnv.stroke_style(gr);
    cnv.stroke();
}

void draw_pixmap(canvas& cnv)
{
    picture pm{get_images_path() + "logo.png" };
    auto x = 250.0f, y = 120.0f;
    cnv.draw(pm, x, y, 0.4);
}

void line_styles(canvas& cnv)
{
    auto where = point{ 500, 200 };
    cnv.shadow_style({ 5.0, 5.0 }, 5, colors::black);

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
    cnv.move_to(where.x, where.y+20);
    cnv.line_to(where.x+100, where.y+20);
    cnv.stroke();

    cnv.stroke_style(colors::light_sea_green);
    cnv.begin_path();
    cnv.line_cap(cnv.square);
    cnv.move_to(where.x, where.y+40);
    cnv.line_to(where.x+100, where.y+40);
    cnv.stroke();

    where.x -= 20;

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
}

void test_draw(canvas& cnv)
{
    background(cnv);
    basics(cnv);
    transformed(cnv);
    linear_gradient(cnv);
    radial_gradient(cnv);
    stroke_gradient(cnv);
    draw_pixmap(cnv);
    line_styles(cnv);
}

float diff_pixel(uint32_t a, uint32_t b)
{
    auto a1 = a & 0xff;
    auto a2 = (a >> 8) & 0xff;
    auto a3 = (a >> 16) & 0xff;
    auto a4 = (a >> 24) & 0xff;

    auto b1 = b & 0xff;
    auto b2 = (b >> 8) & 0xff;
    auto b3 = (b >> 16) & 0xff;
    auto b4 = (b >> 24) & 0xff;

    return float(a1-b1) + float(a2-b2) + float(a3-b3) + float(a4-b4);
}

void compare_golden(picture const& pm, std::string name)
{
    pm.save_png(get_results_path() + name + ".png");
    auto golden = picture(get_golden_path() + name + ".png");
    auto result = picture(get_results_path() + name + ".png");

    auto size = result.bitmap_size();
    CHECK(size == golden.bitmap_size());

    auto a = golden.pixels();
    auto b = result.pixels();

    REQUIRE(a != nullptr);
    REQUIRE(b != nullptr);

    auto diff = 0;
    for (auto i = 0; i != (size.x * size.y); ++i)
        diff += diff_pixel(a[i], b[i]);
    CHECK(diff == 0);
}

void typography(canvas& cnv)
{
    background(cnv);

    cnv.fill_style(rgba(220, 220, 220, 200));
    cnv.stroke_style(rgba(220, 220, 220, 200));

    // Regular
    cnv.font(font_descr{ "Open Sans", 36 });
    cnv.fill_text("Regular", 20, 40);

    // Bold
    cnv.font(font_descr{ "Open Sans", 36 }.bold());
    cnv.fill_text("Bold", 160, 40);

    // Light
    cnv.font(font_descr{ "Open Sans", 36 }.light());
    cnv.fill_text("Light", 250, 40);

    // Italic
    cnv.font(font_descr{ "Open Sans", 36 }.italic());
    cnv.fill_text("Italic", 345, 40);

    // Condensed
    // In this case, the font already describes the condensed 'stretch'
    cnv.font(font_descr{ "Open Sans Condensed", 36 });
    cnv.fill_text("Condensed", 430, 40);

    // Condensed Italic
    // In this case, the font already describes the condensed 'stretch'
    cnv.font(font_descr{ "Open Sans Condensed", 36 }.italic());
    cnv.fill_text("Condensed Italic", 20, 115);

    // Outline
    cnv.font(font_descr{ "Open Sans", 36 }.bold());
    cnv.line_width(0.5);
    cnv.stroke_text("Outline", 210, 115);

    cnv.font(font_descr{ "Open Sans", 52 }.bold());

    // Gradient Fill
    {
        auto gr = canvas::linear_gradient{ { 360, 90 }, { 360, 140 } };
        gr.add_color_stop(0.0, colors::navy_blue);
        gr.add_color_stop(1.0, colors::maroon);
        cnv.fill_style(gr);
        cnv.fill_text("Gradient", 360, 115);
        cnv.stroke_text("Gradient", 360, 115);
    }

    // Outline Gradient Fill
    {
        auto gr = canvas::linear_gradient{ { 360, 165 }, { 360, 215 } };
        gr.add_color_stop(0.0, colors::medium_blue);
        gr.add_color_stop(1.0, colors::medium_violet_red);
        cnv.line_width(1.5);
        cnv.stroke_style(gr);
        cnv.stroke_text("Outline Gradient", 20, 190);
    }

    cnv.font(font_descr{ "Lucida Grande", 52 }.bold());
    cnv.fill_style(rgba(220, 220, 220, 200));
    cnv.fill_text("fi", 500, 190);

    {
        auto state = cnv.new_state();

        // Shadow
        cnv.fill_style(rgba(220, 220, 220, 200));
        cnv.shadow_style({ 5.0, 5.0 }, 5, colors::black);
        cnv.fill_text("Shadow", 20, 265);

        // Glow
        cnv.fill_style(bkd_color);
        cnv.shadow_style(8, colors::light_sky_blue);
        cnv.fill_text("Glow", 250, 265);
    }

    cnv.move_to({ 500, 220 });
    cnv.line_to({ 500, 480 });
    cnv.stroke_style(colors::red);
    cnv.line_width(0.5);
    cnv.stroke();

    cnv.fill_style(rgba(220, 220, 220, 200));
    cnv.font(font_descr{ "Open Sans", 14 });

    char const* align_text[] = {
        "text_align(left)",
        "text_align(right)",
        "text_align(center)",
        "text_align(baseline)",
        "text_align(top)",
        "text_align(middle)",
        "text_align(bottom)"
    };

    int aligns[] = {
        cnv.left,
        cnv.right,
        cnv.center,
        cnv.baseline,
        cnv.top,
        cnv.middle,
        cnv.bottom
    };

    float vspace = 35;
    float vstart = 250-vspace;
    for (int i = 0; i != 7; ++i)
    {
        vstart += vspace;
        cnv.move_to({ 400, vstart });
        cnv.line_to({ 600, vstart });
        cnv.stroke();
        cnv.text_align(aligns[i]);
        cnv.fill_text(align_text[i], 500, vstart);
    }

    std::string text =
        "Although I am a typical loner in daily life, my consciousness of "
        "belonging to the invisible community of those who strive for "
        "truth, beauty, and justice has preserved me from feeling isolated.\n\n"

        "The years of anxious searching in the dark, with their intense "
        "longing, their alternations of confidence and exhaustion, and "
        "final emergence into light—only those who have experienced it "
        "can understand that.\n\n"

        "⁠—Albert Einstein"
        ;

    auto tlayout = text_layout{ font_descr{ "Open Sans", 12 }.italic(), text };
    tlayout.flow(350, true);
    tlayout.draw(cnv, 20, 300);
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

void composite_draw(canvas& cnv, point p, canvas::composite_op_enum mode)
{
    {
        auto save = cnv.new_state();
        cnv.rect({ p.x, p.y, p.x+120, p.y+130 });
        cnv.clip();

        cnv.global_composite_operation(cnv.source_over);
        cnv.fill_style(colors::blue);
        cnv.fill_rect(p.x+20, p.y+20, 60, 60);
        cnv.global_composite_operation(mode);
        cnv.fill_style(colors::red);
        cnv.circle(p.x+70, p.y+70, 30);
        cnv.fill();
    }

    cnv.fill_style(colors::black);
    cnv.text_align(cnv.center);
    cnv.fill_text(mode_name(mode), p.x+60, p.y+110);
}

void composite_ops(canvas& cnv)
{
    cnv.font(font_descr{ "Open Sans", 10 });

    composite_draw(cnv, { 0, 0 }, cnv.source_over);
    composite_draw(cnv, { 120, 0 }, cnv.source_atop);
    composite_draw(cnv, { 240, 0 }, cnv.source_in);
    composite_draw(cnv, { 360, 0 }, cnv.source_out);

    composite_draw(cnv, { 0, 120 }, cnv.destination_over);
    composite_draw(cnv, { 120, 120 }, cnv.destination_atop);
    composite_draw(cnv, { 240, 120 }, cnv.destination_in);
    composite_draw(cnv, { 360, 120 }, cnv.destination_out);

    composite_draw(cnv, { 0, 240 }, cnv.lighter);
    composite_draw(cnv, { 120, 240 }, cnv.darker);
    composite_draw(cnv, { 240, 240 }, cnv.copy);
    composite_draw(cnv, { 360, 240 }, cnv.xor_);
}

// $$$ TODO : Test this $$$
void drop_shadow(canvas& cnv)
{
    background(cnv);

    cnv.shadow_style({ 20, 20 }, 10, colors::black);
    cnv.fill_style(colors::red);
    cnv.fill_rect(20, 20, 100, 80);

    cnv.scale(2, 2);
    cnv.translate(60, 0);
    cnv.shadow_style({ 20, 20 }, 10, colors::black);
    cnv.fill_style(colors::blue);
    cnv.fill_rect(20, 20, 100, 80);
}

///////////////////////////////////////////////////////////////////////////////
// Test Drivers
///////////////////////////////////////////////////////////////////////////////

TEST_CASE("Drawing")
{
    picture pm{ window_size };
    {
        picture_context ctx{pm };
        canvas pm_cnv{ ctx.context() };
        test_draw(pm_cnv);
    }
    compare_golden(pm, "shapes_and_images");
}

TEST_CASE("Typography")
{
    picture pm{ window_size };
    {
        picture_context ctx{pm };
        canvas pm_cnv{ ctx.context() };
        typography(pm_cnv);
    }
    compare_golden(pm, "typography");
}

TEST_CASE("Composite")
{
    picture pm{ window_size };
    {
        picture_context ctx{pm };
        canvas pm_cnv{ ctx.context() };
       composite_ops(pm_cnv);
    }
    compare_golden(pm, "composite_ops");
}

