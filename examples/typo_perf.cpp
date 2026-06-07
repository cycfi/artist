/*=============================================================================
   Copyright (c) 2016-2024 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]

   Typography performance benchmark. A text box that continuously resizes,
   forcing text_layout::flow() + draw() every frame.
=============================================================================*/
#include "app.hpp"

using namespace cycfi::artist;
using namespace font_constants;

auto constexpr window_size = extent{640.0f, 480.0f};
auto constexpr bkd_color   = rgb(44, 42, 45);

static std::string const text =
   "Although I am a typical loner in daily life, my consciousness of "
   "belonging to the invisible community of those who strive for "
   "truth, beauty, and justice has preserved me from feeling isolated.\n\n"

   "The years of anxious searching in the dark, with their intense "
   "longing, their alternations of confidence and exhaustion, and "
   "final emergence into light — only those who have experienced it "
   "can understand that.\n\n"

   "Whoever undertakes to set himself up as a judge of truth and knowledge "
   "is shipwrecked by the laughter of the gods. Intelligence is not the "
   "ability to store information, but to know where to find it.\n\n"

   "⁠— Albert Einstein";

float box_width    = 200.0f;
float width_incr   = 1.5f;
constexpr float min_w = 150.0f;
constexpr float max_w = 580.0f;

void draw(canvas& cnv)
{
   // True window size (before the fit transform), so the FPS readout can anchor
   // to the real window corner instead of moving with the letterbox.
   auto const win = cnv.clip_extent();

   cnv.save();
   scale_to_fit(cnv, {window_size.x, window_size.y}, bkd_color);
   cnv.add_rect({{0, 0}, window_size});
   cnv.fill_style(bkd_color);
   cnv.fill();

   // Animate box width
   box_width += width_incr;
   if (box_width >= max_w || box_width <= min_w)
      width_incr = -width_incr;

   // Draw the animated box outline
   rect const box{20, 20, 20 + box_width, 460};
   cnv.add_rect(box);
   cnv.stroke_style(rgba(100, 120, 200, 100));
   cnv.line_width(1);
   cnv.stroke();

   // Reflow and draw every frame, clipped to the box.
   cnv.save();
   cnv.add_rect(box);
   cnv.clip();
   auto tlayout = text_layout{font_descr{"Open Sans", 14}.italic(), text};
   tlayout.flow(box_width, true);
   tlayout.draw(cnv, {20, 30}, rgba(220, 220, 220, 200));
   cnv.restore();

   cnv.restore();   // undo scale_to_fit → back to window coordinates
   print_elapsed(cnv, {win.width(), win.height()}, bkd_color);
}

int main(int argc, char const* argv[])
{
   return run_app(argc, argv, window_size, bkd_color, true);
}
