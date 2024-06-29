/*=============================================================================
   Copyright (c) 2016-2023 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#if defined(_WIN32)
# ifndef UNICODE
#  define UNICODE
# endif
#endif

#define CATCH_CONFIG_MAIN
#include <infra/catch.hpp>
#include <artist/affine_transform.hpp>
#include "app_paths.hpp"
#include <cmath>
#include <cstdint>

using namespace cycfi::artist;
using namespace font_constants;
using cycfi::codepoint;

auto constexpr window_size = point{640.0f, 480.0f};
auto constexpr bkd_color = rgba(54, 52, 55, 255);

#if defined(ARTIST_QUARTZ_2D)
   // Initialize the fonts and resources
   struct resource_setter
   {
      resource_setter();
   };
   static resource_setter init_resources;
#endif

void background(canvas& cnv)
{
   cnv.add_rect({{0, 0}, window_size});
   cnv.fill_style(bkd_color);
   cnv.fill();
}

void balloon(canvas& cnv)
{
   // quadratic_curve_to
   cnv.begin_path();
   cnv.line_join(cnv.round_join);
   cnv.move_to(75, 25);
   cnv.quadratic_curve_to({25, 25}, {25, 62.5});
   cnv.quadratic_curve_to({25, 100}, {50, 100});
   cnv.quadratic_curve_to({50, 120}, {30, 125});
   cnv.quadratic_curve_to({60, 120}, {65, 100});
   cnv.quadratic_curve_to({125, 100}, {125, 62.5});
   cnv.quadratic_curve_to({125, 25}, {75, 25});
}

void heart(canvas& cnv)
{
   // bezier_curve_to
   constexpr float y0 = 40, y1 = 50, y2 = 23, y3 = 90, y4 = 45, y5 = 75, y6 = 125;
   cnv.begin_path();
   cnv.move_to({75, y0});
   cnv.bezier_curve_to({75, y4}, {70, y2}, {50, y2});
   cnv.bezier_curve_to({20, y2}, {20, y1}, {20, y1});
   cnv.bezier_curve_to({20, y5}, {y0, y3}, {75, y6});
   cnv.bezier_curve_to({110, y3}, {130, y5}, {130, y1});
   cnv.bezier_curve_to({ 130, y1}, {130, y2}, {100, y2});
   cnv.bezier_curve_to({85, y2}, {75, y4}, {75, y0});
   cnv.fill_style(color{0.2, 0, 0}.opacity(0.4));
}

void basics(canvas& cnv)
{
   auto state = cnv.new_state();

   cnv.add_rect(20, 20, 80, 40);
   cnv.fill_style(colors::navy_blue);
   cnv.fill_preserve();

   cnv.stroke_style(colors::antique_white.opacity(0.8));
   cnv.line_width(0.5);
   cnv.stroke();

   cnv.add_round_rect(40, 35, 80, 45, 8);
   cnv.fill_style(colors::light_sea_green.opacity(0.5));
   cnv.fill();

   cnv.move_to(10, 10);
   cnv.line_to(100, 100);
   cnv.line_width(10);
   cnv.stroke_style(colors::hot_pink);
   cnv.stroke();

   cnv.add_circle(120, 80, 40);
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
   cnv.stroke_style(color{0.8, 0, 0});
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

namespace detail
{
   struct corner_radii
   {
      float top_left, top_right, bottom_right, bottom_left;
      corner_radii operator+(float v) const;
      corner_radii operator-(float v) const;
   };
}

void draw_round_rect(canvas& cnv, rect bounds, detail::corner_radii radius)
{
   auto l = bounds.left;
   auto t = bounds.top;
   auto r = bounds.right;
   auto b = bounds.bottom;
   radius.top_left =     std::min(radius.top_left,     std::min(bounds.width(), bounds.height()) / 2);
   radius.top_right =    std::min(radius.top_right,    std::min(bounds.width(), bounds.height()) / 2);
   radius.bottom_right = std::min(radius.bottom_right, std::min(bounds.width(), bounds.height()) / 2);
   radius.bottom_left =  std::min(radius.bottom_left,  std::min(bounds.width(), bounds.height()) / 2);

   cnv.begin_path();
   cnv.arc({r-radius.bottom_right, b-radius.bottom_right}, radius.bottom_right, 0,        M_PI*0.5);
   cnv.arc({l+radius.bottom_left,  b-radius.bottom_left }, radius.bottom_left,  M_PI*0.5, M_PI    );
   cnv.arc({l+radius.top_left,     t+radius.top_left    }, radius.top_left,     M_PI,     M_PI*1.5);
   cnv.arc({r-radius.top_right,    t+radius.top_right   }, radius.top_right,    M_PI*1.5, 0       );
   cnv.close_path();
}

void basics2(canvas& cnv)
{
   auto state = cnv.new_state();
   rect bounds = {20, 20, 220, 120};

   {
      draw_round_rect(cnv, bounds, {-1, 40, 40, -1}); // Negative radius

      cnv.fill_style(colors::light_sea_green.opacity(0.8));
      cnv.fill_preserve();

      cnv.stroke_style(colors::antique_white.opacity(0.8));
      cnv.line_width(2);
      cnv.stroke();
   }
   {
      draw_round_rect(cnv, bounds.move(20, 20), {15, 40, 40, 15});

      cnv.fill_style(colors::navy_blue.opacity(0.5));
      cnv.fill_preserve();

      cnv.stroke_style(colors::antique_white.opacity(0.5));
      cnv.line_width(2);
      cnv.stroke();
   }
   {
      draw_round_rect(cnv, bounds.move(40, 40), {0, 0, 0, 0});

      cnv.fill_style(colors::indian_red.opacity(0.5));
      cnv.fill_preserve();

      cnv.stroke_style(colors::antique_white.opacity(0.5));
      cnv.line_width(2);
      cnv.stroke();
   }
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
   auto gr = canvas::linear_gradient{x, y, x+300, y};
   rainbow(gr);

   cnv.add_round_rect(x, y, 300, 80, 5);
   cnv.fill_style(gr);
   cnv.fill();
}

void radial_gradient(canvas& cnv)
{
   auto center = point{475, 90};
   auto radius = 75.0f;
   auto gr = canvas::radial_gradient{center, 5, center.move(15, 10), radius};
   gr.add_color_stop(0.0, colors::red);
   gr.add_color_stop(1.0, colors::black);

   cnv.add_circle({center.move(15, 10), radius - 10});
   cnv.fill_style(gr);
   cnv.fill();
}

void stroke_gradient(canvas& cnv)
{
   auto x = 300.0f;
   auto y = 20.0f;
   auto gr = canvas::linear_gradient{x, y, x+300, y+80};
   gr.add_color_stop(0.0, colors::navy_blue);
   gr.add_color_stop(1.0, colors::maroon);

   cnv.add_round_rect(x, y, 300, 80, 5);
   cnv.line_width(8);
   cnv.stroke_style(gr);
   cnv.stroke();
}

void draw_pixmap(canvas& cnv)
{
   image pm{get_images_path() + "logo.png"};
   auto x = 250.0f, y = 120.0f;
   cnv.draw(pm, x, y, 0.4);
}

void line_styles(canvas& cnv)
{
   auto where = point{500, 200};
   cnv.shadow_style({5.0, 5.0}, 5, colors::black);

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

void test_draw2(canvas& cnv)
{
   background(cnv);
   basics2(cnv);
}

// Function to extract RGBA values with generic component names Note: Using
// generic names (x, y, z, w) to make the code agnostic about the order of
// components within uint32_t values.
void extract_rgba(uint32_t pixel, uint8_t& x, uint8_t& y, uint8_t& z, uint8_t& w)
{
   x = (pixel >> 24) & 0xFF;
   y = (pixel >> 16) & 0xFF;
   z = (pixel >> 8) & 0xFF;
   w = pixel & 0xFF;
}

// Function to calculate structural similarity index (SSI) between two images
double calculate_ssi(uint32_t const  img1[], uint32_t const img2[], int const width, int const height)
{
   const double c1 = 0.0001; // Small constant to avoid division by zero
   const double c2 = 0.0009; // Small constant to avoid division by zero
   double mean_x = 0.0, mean_y = 0.0, sigma_x = 0.0, sigma_y = 0.0, sigma_xy = 0.0;

   for (int i = 0; i < width * height; ++i)
   {
      // Extract individual components using generic names x, y, z, w
      uint8_t x1, y1, z1, w1;
      uint8_t x2, y2, z2, w2;

      extract_rgba(img1[i], x1, y1, z1, w1);
      extract_rgba(img2[i], x2, y2, z2, w2);

      mean_x += x1 + y1 + z1 + w1;
      mean_y += x2 + y2 + z2 + w2;
   }

   mean_x /= (width * height * 4);
   mean_y /= (width * height * 4);

   for (int i = 0; i < width * height; ++i)
   {
      // Extract individual components using generic names x, y, z, w
      uint8_t x1, y1, z1, w1;
      uint8_t x2, y2, z2, w2;

      extract_rgba(img1[i], x1, y1, z1, w1);
      extract_rgba(img2[i], x2, y2, z2, w2);

      double dev_x = x1 - mean_x;
      double dev_y = x2 - mean_y;
      sigma_x += dev_x * dev_x;
      sigma_y += dev_y * dev_y;
      sigma_xy += dev_x * dev_y;

      dev_x = y1 - mean_x;
      dev_y = y2 - mean_y;
      sigma_x += dev_x * dev_x;
      sigma_y += dev_y * dev_y;
      sigma_xy += dev_x * dev_y;

      dev_x = z1 - mean_x;
      dev_y = z2 - mean_y;
      sigma_x += dev_x * dev_x;
      sigma_y += dev_y * dev_y;
      sigma_xy += dev_x * dev_y;

      dev_x = w1 - mean_x;
      dev_y = w2 - mean_y;
      sigma_x += dev_x * dev_x;
      sigma_y += dev_y * dev_y;
      sigma_xy += dev_x * dev_y;
   }

   sigma_x /= (width * height * 4 - 1);
   sigma_y /= (width * height * 4 - 1);
   sigma_xy /= (width * height * 4 - 1);

   const double numerator = (2 * mean_x * mean_y + c1) * (2 * sigma_xy + c2);
   const double denominator = (mean_x * mean_x + mean_y * mean_y + c1) * (sigma_x + sigma_y + c2);

   return numerator / denominator;
}

void compare_golden(image const& pm, std::string name)
{
   pm.save_png(get_results_path() + name + ".png");
   auto golden = image(get_golden_path() + name + ".png");
   auto result = image(get_results_path() + name + ".png");

   auto bm_size = result.bitmap_size();
   CHECK(bm_size == golden.bitmap_size());

   auto size = result.size();
   CHECK(size.x == Approx(window_size.x));
   CHECK(size.y == Approx(window_size.y));

   auto a = golden.pixels();
   auto b = result.pixels();

   REQUIRE(a != nullptr);
   REQUIRE(b != nullptr);

   auto ssi = calculate_ssi(a, b, bm_size.x, bm_size.y);
   CHECK(ssi > 0.985);
   std::cout << "SSI result for " << name << " : " << ssi << std::endl;
}

void typography(canvas& cnv)
{
   background(cnv);

   cnv.fill_style(rgba(220, 220, 220, 200));
   cnv.stroke_style(rgba(220, 220, 220, 200));

   // Regular
   cnv.font(font_descr{"Open Sans", 36});
   cnv.fill_text("Regular", 20, 40);

   // Bold
   cnv.font(font_descr{"Open Sans", 36}.bold());
   cnv.fill_text("Bold", 160, 40);

   // Light
   cnv.font(font_descr{"Open Sans", 36}.light());
   cnv.fill_text("Light", 250, 40);

   // Italic
   cnv.font(font_descr{"Open Sans", 36}.italic());
   cnv.fill_text("Italic", 345, 40);

   // Condensed
   cnv.font(font_descr{"Open Sans Condensed, Open Sans", 36}.condensed());
   cnv.fill_text("Condensed", 430, 40);

   // Condensed Italic
   cnv.font(font_descr{"Open Sans Condensed, Open Sans", 36}.italic().condensed());
   cnv.fill_text("Condensed Italic", 20, 115);

   // In the last two cases, the font family 'Open Sans Condensed' already
   // describes the font as condensed, but we still add the font family 'Open
   // Sans' and styles because, depending on platform, the font family 'Open
   // Sans Condensed' is either grouped into the 'Open Sans' family (e.g.
   // Windows), or separate as a distinct family (e.g. MacOS).

   // Outline
   cnv.font(font_descr{"Open Sans", 36}.bold());
   cnv.line_width(0.5);
   cnv.stroke_text("Outline", 210, 115);

   cnv.font(font_descr{"Open Sans", 52}.bold());

   // Gradient Fill
   {
      auto gr = canvas::linear_gradient{{360, 90}, {360, 140}};
      gr.add_color_stop(0.0, colors::navy_blue);
      gr.add_color_stop(1.0, colors::maroon);
      cnv.fill_style(gr);
      cnv.fill_text("Gradient", 360, 115);
      cnv.stroke_text("Gradient", 360, 115);
   }

   // Outline Gradient Fill
   {
      auto gr = canvas::linear_gradient{{360, 165}, {360, 215}};
      gr.add_color_stop(0.0, colors::medium_blue);
      gr.add_color_stop(1.0, colors::medium_violet_red);
      cnv.line_width(1.5);
      cnv.stroke_style(gr);
      cnv.stroke_text("Outline Gradient", 20, 190);
   }

#if defined(ARTIST_QUARTZ_2D) // CoreText supports ligatures right out of the box, but only for some fonts
   cnv.font(font_descr{"Lucida Grande", 52}.bold());
#else
   cnv.font(font_descr{"Open Sans", 52}.bold());
#endif

   cnv.fill_style(rgba(220, 220, 220, 200));
   cnv.fill_text("fi fl", 500, 190);

   cnv.font(font_descr{"Open Sans", 52}.weight(semi_bold));
   {
      auto state = cnv.new_state();

      // Shadow
      cnv.fill_style(rgba(220, 220, 220, 200));
      cnv.shadow_style({5.0, 5.0}, 5, colors::black);
      cnv.fill_text("Shadow", 20, 265);

      // Glow
      cnv.fill_style(bkd_color);
      cnv.shadow_style(8, colors::light_sky_blue);
      cnv.fill_text("Glow", 250, 265);

      auto m = cnv.measure_text("Shadow");
      CHECK(std::floor(m.ascent) == 55);
      CHECK(std::floor(m.descent) == 15);
      CHECK(std::floor(m.leading) == 0);
      CHECK(std::floor(m.size.x) == 198);
      CHECK(std::floor(m.size.y) == 70);
   }

   cnv.move_to({500, 220});
   cnv.line_to({500, 480});
   cnv.stroke_style(colors::red);
   cnv.line_width(0.5);
   cnv.stroke();

   cnv.fill_style(rgba(220, 220, 220, 200));
   cnv.font(font_descr{"Open Sans", 14});

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
      cnv.move_to({400, vstart});
      cnv.line_to({600, vstart});
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

   auto tlayout = text_layout{
      font_descr{"Open Sans", 12}.italic()
    , text
   };
   tlayout.flow(350, true);
   tlayout.draw(cnv, 20, 300, rgba(220, 220, 220, 200));

   // Hit testing
   {
      auto i = tlayout.caret_index(0, 2000);
      CHECK(i == tlayout.npos);

      i = tlayout.caret_index(0, 0);
      CHECK(i == 0);

      i = tlayout.caret_index(350, 0);
      CHECK(i == 64);
      CHECK(text[i] == ' ');

      i = tlayout.caret_index(0, 20);
      CHECK(i == 133);
      CHECK(text[i] == 'b');

      i = tlayout.caret_index(5, 20);
      CHECK(i == 134);
      CHECK(text[i] == 'e');

      i = tlayout.caret_index(350, 20);
      CHECK(i == 192);
      CHECK(text[i] == '\n');

      i = tlayout.caret_index(109, 20);
      CHECK(i == 154);
      CHECK(text[i] == 'a');

      i = tlayout.caret_index(350, 15);
      CHECK(i == 132);
      CHECK(text[i] == ' ');

      // Harfbuzz vs. CoreText have slightly different results here,
      // but that is OK.
      i = tlayout.caret_index(343, 15);
      auto check_index = i == 131 || i == 130;
      auto check_char = text[i] == ',' || text[i] == 'h';
      CHECK(check_index);
      CHECK(check_char);

      i = tlayout.caret_index(20, 49);
      CHECK(i == 193);
      CHECK(text[i] == '\n');

      i = tlayout.caret_index(0, 147);
      CHECK(i == 403);
      char32_t cp = *(tlayout.text().data()+i);
      CHECK(cp == 8288); // 'WORD JOINER' (U+2060)

      i = tlayout.caret_index(88, 147);
      CHECK(i == tlayout.text().size());
   }

   // glyph_bounds
   {
      auto test_caret_pos =
         [&tlayout](std::size_t index, bool exceed = false)
         {
            point pos = tlayout.caret_point(index);
            if (exceed)
            {
               CHECK(std::floor(pos.x) == 86);
               CHECK(std::floor(pos.y) == 147);
            }
            else
            {
               std::size_t got_index = tlayout.caret_index(pos);
               CHECK(got_index == index);
            }
         };

      test_caret_pos(1000, true);
      test_caret_pos(-1, true);

      test_caret_pos(10);
      test_caret_pos(0);
      test_caret_pos(64);
      test_caret_pos(193);
      test_caret_pos(132);
      test_caret_pos(133);
      test_caret_pos(154);
      test_caret_pos(325);
      test_caret_pos(326);
      test_caret_pos(405);
      test_caret_pos(420);
   }
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

      case canvas::difference:        return "difference";
      case canvas::exclusion:         return "exclusion";
      case canvas::multiply:          return "multiply";
      case canvas::screen:            return "screen";

      case canvas::color_dodge:       return "color_dodge";
      case canvas::color_burn:        return "color_burn";
      case canvas::soft_light:        return "soft_light";
      case canvas::hard_light:        return "hard_light";

      case canvas::hue:               return "hue";
      case canvas::saturation:        return "saturation";
      case canvas::color_op:          return "color_op";
      case canvas::luminosity:        return "luminosity";

      default: return "";
   };
}

void composite_draw(canvas& cnv, point p, canvas::composite_op_enum mode)
{
   {
      auto save = cnv.new_state();
      cnv.add_rect({p.x, p.y, p.x + 120, p.y + 130});
      cnv.clip();

      cnv.global_composite_operation(cnv.source_over);
      cnv.fill_style(colors::blue);
      cnv.fill_rect(p.x+20, p.y+20, 60, 60);
   }

   {
      auto save = cnv.new_state();
      image pm{110, 110};
      {
         offscreen_image ctx{pm};
         canvas pm_cnv{ctx.context()};
         pm_cnv.fill_style(colors::red);
         pm_cnv.add_circle(70, 70, 30);
         pm_cnv.fill();
      }

      cnv.global_composite_operation(mode);
      cnv.draw(pm, p);
   }

   cnv.fill_style(colors::black);
   cnv.text_align(cnv.center);
   cnv.fill_text(mode_name(mode), p.x+60, p.y+110);
}

void composite_ops(canvas& cnv)
{
   cnv.font(font_descr{"Open Sans", 10});

   composite_draw(cnv, {0, 0}, cnv.source_over);
   composite_draw(cnv, {120, 0}, cnv.source_atop);
   composite_draw(cnv, {240, 0}, cnv.source_in);
   composite_draw(cnv, {360, 0}, cnv.source_out);

   composite_draw(cnv, {0, 120}, cnv.destination_over);
   composite_draw(cnv, {120, 120}, cnv.destination_atop);
   composite_draw(cnv, {240, 120}, cnv.destination_in);
   composite_draw(cnv, {360, 120}, cnv.destination_out);

   composite_draw(cnv, {0, 240}, cnv.lighter);
   composite_draw(cnv, {120, 240}, cnv.darker);
   composite_draw(cnv, {240, 240}, cnv.copy);
   composite_draw(cnv, {360, 240}, cnv.xor_);
}

void drop_shadow(canvas& cnv)
{
   cnv.shadow_style({20, 20}, 10, colors::black);
   cnv.fill_style(colors::red);
   cnv.fill_rect(20, 20, 100, 80);

   cnv.scale(2, 2);
   cnv.translate(60, 0);
   cnv.shadow_style({20, 20}, 10, colors::black);
   cnv.fill_style(colors::blue);
   cnv.fill_rect(20, 20, 100, 80);
}

void paths(canvas& cnv)
{
   auto stroke_fill =
      [&](path const& p, color fill_c, color stroke_c)
      {
         cnv.add_path(p);
         cnv.fill_color(fill_c);
         cnv.stroke_color(stroke_c);
         cnv.fill();
         cnv.line_width(5);
         cnv.add_path(p);
         cnv.stroke();
      };

   auto stroke =
      [&](path const& p, color stroke_c)
      {
         cnv.add_path(p);
         cnv.stroke_color(stroke_c);
         cnv.line_width(5);
         cnv.stroke();
      };

   auto dot =
      [&](float x, float y)
      {
         cnv.add_circle(x, y, 10);
         cnv.fill_color(colors::white.opacity(0.5));
         cnv.fill();
      };

   background(cnv);

   // These paths are defined using SVG-style strings lifted off the
   // W3C SVG documentation: https://www.w3.org/TR/SVG/paths.html

   {
      auto save = cnv.new_state();
      cnv.scale(0.5, 0.5);

      cnv.translate(-40, 0);
      path p1{"M 100 100 L 300 100 L 200 300 z"};
      stroke_fill(p1, colors::green.opacity(0.5), colors::ivory);

      cnv.translate(220, 0);
      path p2{"M100,200 C100,100 250,100 250,200 S400,300 400,200"};
      stroke(p2, colors::light_sky_blue);

      cnv.translate(-150, 250);
      path p3{"M200,300 Q400,50 600,300 T1000,300"};
      stroke(p3, colors::light_sky_blue);

      path p4{"M200,300 L400,50 L600,300 L800,550 L1000,300"};
      stroke(p4, colors::light_gray.opacity(0.5));
      dot(200, 300);
      dot(600, 300);
      dot(1000, 300);
      dot(400, 50);
      dot(800, 550);
      cnv.translate(150, -250);

      cnv.translate(350, 0);
      path p5{"M300,200 h-150 a150,150 0 1,0 150,-150 z"};
      stroke_fill(p5, colors::red.opacity(0.8), colors::ivory);

      path p6{"M275,175 v-150 a150,150 0 0,0 -150,150 z"};
      stroke_fill(p6, colors::blue.opacity(0.8), colors::ivory);

      cnv.translate(-350, 200);
      path p7{
         "M600,350 l 50,-25"
         "a25,25 -30 0,1 50,-25 l 50,-25"
         "a25,50 -30 0,1 50,-25 l 50,-25"
         "a25,75 -30 0,1 50,-25 l 50,-25"
         "a25,100 -30 0,1 50,-25 l 50,-25"
      };
      stroke(p7, colors::ivory);
   }
}

void compare_transform(affine_transform const& a, affine_transform const& b)
{
   CHECK(a.a == Approx(b.a));
   CHECK(a.b == Approx(b.b));
   CHECK(a.c == Approx(b.c));
   CHECK(a.d == Approx(b.d));
   CHECK(a.tx == Approx(b.tx));
   CHECK(a.ty == Approx(b.ty));
}

void misc(canvas& cnv)
{
   using cycfi::pi;

   background(cnv);
   cnv.clear_rect(30, 30, 50, 50);

   {
      auto save = cnv.new_state();

      affine_transform mat = cnv.transform();
      cnv.fill_style(colors::blue);

      cnv.rotate(pi/4);
      mat = mat.rotate(pi/4);
      auto ctm = cnv.transform();
      compare_transform(mat, ctm);

      cnv.scale(2);
      mat = mat.scale(2);
      ctm = cnv.transform();
      compare_transform(mat, ctm);

      cnv.translate(100, 0);
      mat = mat.translate(100, 0);
      ctm = cnv.transform();
      compare_transform(mat, ctm);

      cnv.add_rect(-25, -25, 50, 50);
      cnv.fill();

      cnv.transform(mat);
      cnv.add_rect(-20, -20, 40, 40);
      cnv.fill_style(colors::red);
      cnv.fill();
   }

   {
      affine_transform mat;
      auto p = mat.apply(0.0, 0.0);
      CHECK(p.x == Approx(0));
      CHECK(p.y == Approx(0));

      mat = mat.scale(10);
      p = mat.apply(4, 4);
      CHECK(p.x == Approx(40));
      CHECK(p.y == Approx(40));

      mat = mat.translate(2, 2);
      p = mat.apply(0.0, 0.0);
      CHECK(p.x == Approx(20));
      CHECK(p.y == Approx(20));
   }

   {
      auto save = cnv.new_state();

      affine_transform mat = cnv.transform();
      mat = mat.translate(144, 144);
      mat = mat.skew(pi/8, pi/12);
      cnv.transform(mat);
      cnv.add_rect(0, 0, 72, 72);
      cnv.fill_style(colors::green);
      cnv.fill();
   }

   {
      path p;
      p.add_circle(circle{230, 230, 50});
      p.add_circle(circle{230, 230, 25});
      p.fill_rule(path::fill_odd_even);

      CHECK(p.includes(230-50+5, 230));
      CHECK(!p.includes(230, 230));

      cnv.clip(p);
      cnv.add_rect(0, 0, 500, 500);
      cnv.fill_style(colors::navajo_white.opacity(0.5));
      cnv.fill_preserve();

      CHECK(cnv.point_in_path(10, 10));
      CHECK(!p.includes(501, 500));
   }

   // Test small offscreen hit testing and text measurements work
   {
      image img{1, 1};
      offscreen_image offscr{img};
      canvas cnv{offscr.context()};

      cnv.add_circle(230, 230, 50);
      cnv.add_circle(230, 230, 25);
      cnv.fill_rule(path::fill_odd_even);

      CHECK(cnv.point_in_path(230-50+5, 230));
      CHECK(!cnv.point_in_path(230, 230));

      cnv.font(font_descr{"Open Sans", 36});
      auto m = cnv.measure_text("Hello, World");
      CHECK(std::abs(m.size.x-205.0f) <= 1.0);
      CHECK(std::abs(m.size.y-49) <= 1.0);
      CHECK(std::floor(m.ascent) == 38);
      CHECK(std::floor(m.descent) == 10);
      CHECK(std::floor(m.leading) == 0);
   }
}

///////////////////////////////////////////////////////////////////////////////
namespace cycfi::artist
{
   void init_paths()
   {}

   // This is declared in font.hpp
   fs::path get_user_fonts_directory()
   {
      return get_fonts_path();
   }
}

///////////////////////////////////////////////////////////////////////////////
// Test Drivers
///////////////////////////////////////////////////////////////////////////////

// How to add a test:
//    1. Copy one of the test cases
//    2. Change the target function to be tested (e.g. test_draw(pm_cnv))
//    3. Run it. You will get a test failure on the first run.
//    4. Inspect the PNG result in the output "results" directory in the
//       output test folder generated by CMake
//    5. If the result is good, place it in the golden directory alongside
//       the souure test files. There's one for each platform and graphics
//       backend, e.g. "test/macos_golden/quartz_2d"
//    6. Have CMake regenerate the build files

TEST_CASE("Drawing")
{
   image pm{window_size};
   {
      offscreen_image ctx{pm};
      canvas pm_cnv{ctx.context()};
      test_draw(pm_cnv);
   }
   compare_golden(pm, "shapes_and_images");
}

TEST_CASE("Drawing2")
{
   image pm{window_size};
   {
      offscreen_image ctx{pm};
      canvas pm_cnv{ctx.context()};
      test_draw2(pm_cnv);
   }
   compare_golden(pm, "shapes2");
}

TEST_CASE("Typography")
{
   image pm{window_size};
   {
      offscreen_image ctx{pm};
      canvas pm_cnv{ctx.context()};

      auto p = pm_cnv.device_to_user(100, 100);

      typography(pm_cnv);
   }
   compare_golden(pm, "typography");
}

TEST_CASE("Composite")
{
   image pm{window_size};
   {
      offscreen_image ctx{pm};
      canvas pm_cnv{ctx.context()};
      composite_ops(pm_cnv);
   }
   compare_golden(pm, "composite_ops");
}

TEST_CASE("DropShadow")
{
   image pm{window_size};
   {
      offscreen_image ctx{pm};
      canvas pm_cnv{ctx.context()};
      drop_shadow(pm_cnv);
   }
   compare_golden(pm, "drop_shadow");
}

TEST_CASE("Paths")
{
   image pm{window_size};
   {
      offscreen_image ctx{pm};
      canvas pm_cnv{ctx.context()};
      paths(pm_cnv);
   }
   compare_golden(pm, "paths");
}

TEST_CASE("Misc")
{
   image pm{window_size};
   {
      offscreen_image ctx{pm};
      canvas pm_cnv{ctx.context()};
      misc(pm_cnv);
   }
   compare_golden(pm, "misc");
}


TEST_CASE("Color Maths")
{
  color a(0.2, 0.25, 0.5, 1.0);
  color b(0.5, 0.6, 0.1, 1.0);

  auto check = [&](color result, color check)
  {
    REQUIRE_THAT(result.red, Catch::WithinRel(check.red, 0.001f));
    REQUIRE_THAT(result.green, Catch::WithinRel(check.green, 0.001f));
    REQUIRE_THAT(result.blue, Catch::WithinRel(check.blue, 0.001f));
    REQUIRE_THAT(result.alpha, Catch::WithinRel(check.alpha, 0.001f));
  };

  check(a + b, color(0.7, 0.85, 0.6, 1.0));
  check(a - b, color(-0.3, -0.35, 0.4, 1.0));
  check(a * 2, color(0.4, 0.5, 1.0, 1.0));
  check(2 * a, color(0.4, 0.5, 1.0, 1.0));
}
