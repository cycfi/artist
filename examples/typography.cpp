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

void typography(canvas& cnv)
{
   background(cnv);

   cnv.fill_style(rgba(220, 220, 220, 200));
   cnv.stroke_style(rgba(220, 220, 220, 200));

   // Regular
   cnv.font(font_descr{ "Open Sans", 36 });
   cnv.fill_text("Regular", { 20, 40 });

   // Bold
   cnv.font(font_descr{ "Open Sans", 36 }.bold());
   cnv.fill_text("Bold", { 160, 40 });

   // Light
   cnv.font(font_descr{ "Open Sans", 36 }.light());
   cnv.fill_text("Light", { 250, 40 });

   // Italic
   cnv.font(font_descr{ "Open Sans", 36 }.italic());
   cnv.fill_text("Italic", { 345, 40 });

   // Condensed
   // In this case, the font already describes the condensed 'stretch'
   cnv.font(font_descr{ "Open Sans Condensed", 36 });
   cnv.fill_text("Condensed", { 430, 40 });

   // Condensed Italic
   // In this case, the font already describes the condensed 'stretch'
   cnv.font(font_descr{ "Open Sans Condensed", 36 }.italic());
   cnv.fill_text("Condensed Italic", { 20, 115 });

   // Outline
   cnv.font(font_descr{ "Open Sans", 36 }.bold());
   cnv.line_width(0.5);
   cnv.stroke_text("Outline", { 210, 115 });

   cnv.font(font_descr{ "Open Sans", 52 }.bold());
   // Gradient Fill
   {
      auto gr = canvas::linear_gradient{ { 360, 90 }, { 360, 140 } };
      gr.add_color_stop(0.0, colors::navy_blue);
      gr.add_color_stop(1.0, colors::maroon);
      cnv.fill_style(gr);
      cnv.fill_text("Gradient", { 360, 115 });
      cnv.stroke_text("Gradient", { 360, 115 });
   }

   // Outline Gradient Fill
   {
      auto gr = canvas::linear_gradient{ { 360, 165 }, { 360, 215 } };
      gr.add_color_stop(0.0, colors::medium_blue);
      gr.add_color_stop(1.0, colors::medium_violet_red);
      cnv.line_width(1.5);
      cnv.stroke_style(gr);
      cnv.stroke_text("Outline Gradient", { 20, 190 });
   }

   cnv.font(font_descr{ "Lucida Grande", 52 }.bold());
   cnv.fill_style(rgba(220, 220, 220, 200));
   cnv.fill_text("fi", 500, 190);

   {
      auto state = cnv.new_state();

      // Shadow
      cnv.fill_style(rgba(220, 220, 220, 200));
      cnv.shadow_style({ 5.0, 5.0 }, 5, colors::black);
      cnv.fill_text("Shadow", { 20, 265 });

      // Glow
      cnv.fill_style(bkd_color);
      cnv.shadow_style(8, colors::light_sky_blue);
      cnv.fill_text("Glow", { 250, 265 });
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
      cnv.fill_text(align_text[i], { 500, vstart });
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
   tlayout.draw(cnv, { 20, 300 });
}

void draw(canvas& cnv)
{
   typography(cnv);
}

int main(int argc, const char* argv[])
{
   return run_app(argc, argv, window_size);
}

