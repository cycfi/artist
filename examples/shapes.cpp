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

void rectangles(canvas& cnv)
{
   constexpr auto x = 40;
   constexpr auto y = 40;
   cnv.line_width(2);

   cnv.rect(x, y, 150, 100);
   cnv.fill_style(colors::red);
   cnv.fill();

   cnv.rect(x, y, 150, 100);
   cnv.stroke_style(colors::navajo_white.opacity(0.5));
   cnv.stroke();

   cnv.round_rect(x+30, y+30, 150, 100, 10);
   cnv.fill_style(colors::blue.opacity(0.5));
   cnv.fill();

   cnv.round_rect(x+30, y+30, 150, 100, 10);
   cnv.stroke_style(colors::honeydew.opacity(0.5));
   cnv.stroke();
}

void circles_and_arcs(canvas& cnv)
{
   constexpr auto x = 300;
   constexpr auto y = 100;

   for (int i = 0; i != 7; ++i)
   {
      auto cx = x+(i*45);
      auto cy = y;
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
   constexpr auto x = 40;
   constexpr auto y = 200;
   constexpr auto r = 40;

   auto make_arc =
      [=](canvas& cnv)
      {
         cnv.begin_path();
         cnv.move_to(x, y);
         cnv.line_to(x+(80-r), y);
         cnv.arc_to(x+80, y, x+80, y+20, r);
         cnv.line_to(x+80, y+80);
      };

   cnv.stroke_style(colors::dodger_blue.opacity(0.6));
   cnv.line_width(10);
   make_arc(cnv);
   cnv.stroke();

   cnv.fill_style(colors::dodger_blue.opacity(0.2));
   make_arc(cnv);
   cnv.line_to(x, y+80);
   cnv.close();
   cnv.fill();

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
   auto where = point{ 160, 215 };
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
   auto where = point{ 290, 100 };
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

void bezier(canvas& cnv)
{
   auto where = point{ 450, 280 };
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

void quad(canvas& cnv)
{
   auto where = point{ 40, 420 };
   auto height = 100;
   auto width = 120;
   auto start = point{ where.x, where.y };
   auto cp = point{ where.x+(width/2), where.y-height };
   auto end = point{ where.x+width, where.y };

   cnv.line_width(10);
   cnv.stroke_style(colors::dodger_blue.opacity(0.6));
   cnv.line_cap(cnv.round);

   cnv.begin_path();
   cnv.move_to(start);
   cnv.quadratic_curve_to(cp, end);
   cnv.stroke();

   cnv.stroke_style(colors::pink.opacity(0.2));
   cnv.circle(cp.x, cp.y, 3);
   cnv.fill();

   cnv.stroke_style(colors::white);
   cnv.line_width(1);
   cnv.move_to(start);
   cnv.line_to(cp);
   cnv.line_to(end);
   cnv.stroke();
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
   auto y = 330.0f;
   auto gr = canvas::linear_gradient{ x, y, x+300, y };
   rainbow(gr);

   cnv.round_rect(x, y, 300, 80, 5);
   cnv.fill_style(gr);
   cnv.fill();
}

void radial_gradient(canvas& cnv)
{
   auto center = point{ 208, 360 };
   auto radius = 65.0f;
   auto gr = canvas::radial_gradient{ center, 5, center.move(15, 10), radius };
   gr.add_color_stop(0.0, colors::red);
   gr.add_color_stop(1.0, colors::black);

   cnv.circle({ center.move(15, 10), radius-10 });
   cnv.fill_style(gr);
   cnv.fill();
}

void draw(canvas& cnv)
{
   background(cnv);
   rectangles(cnv);
   circles_and_arcs(cnv);
   arc_to(cnv);
   line_caps(cnv);
   line_joins(cnv);
   bezier(cnv);
   quad(cnv);
   linear_gradient(cnv);
   radial_gradient(cnv);
}

int main(int argc, const char* argv[])
{
   return run_app(argc, argv, window_size);
}

