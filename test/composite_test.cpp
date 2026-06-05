#include "test_support.hpp"

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

void composite_ops2(canvas& cnv)
{
   cnv.font(font_descr{"Open Sans", 10});

   composite_draw(cnv, {0, 0},   cnv.difference);
   composite_draw(cnv, {120, 0}, cnv.exclusion);
   composite_draw(cnv, {240, 0}, cnv.multiply);
   composite_draw(cnv, {360, 0}, cnv.screen);

   composite_draw(cnv, {0, 130},   cnv.color_dodge);
   composite_draw(cnv, {120, 130}, cnv.color_burn);
   composite_draw(cnv, {240, 130}, cnv.soft_light);
   composite_draw(cnv, {360, 130}, cnv.hard_light);

   composite_draw(cnv, {0, 260},   cnv.hue);
   composite_draw(cnv, {120, 260}, cnv.saturation);
   composite_draw(cnv, {240, 260}, cnv.color_op);
   composite_draw(cnv, {360, 260}, cnv.luminosity);
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

auto constexpr tauri_bkd = rgb(44, 42, 45);

void tauri_logo(canvas& cnv)
{
   cnv.begin_path();
   cnv.move_to(24.091255, 16.26616);
   cnv.line_to(44.374182, 43);
   cnv.bezier_curve_to(44.374182, 43, 34.488091, 43.011495, 34.488091, 43.011495);
   cnv.bezier_curve_to(34.488091, 43.011495, 35.642931, 41.930399, 36.249098, 41.25);
   cnv.bezier_curve_to(37.686127, 39.636989, 37.287291, 38.778661, 31.154085, 30.285105);
   cnv.bezier_curve_to(27.494338, 25.216912, 24.275, 21.070209, 24, 21.070209);
   cnv.bezier_curve_to(23.725, 21.070209, 20.505662, 25.216912, 16.845915, 30.285105);
   cnv.bezier_curve_to(10.712709, 38.778661, 10.313873, 39.636989, 11.750902, 41.25);
   cnv.bezier_curve_to(12.342655, 41.91422, 13.259451, 42.993021, 13.259451, 42.993021);
   cnv.line_to(3.6258176, 43);
   cnv.bezier_curve_to(3.6258176, 43, 24.091255, 16.26616, 24.091255, 16.26616);
   cnv.fill();
   cnv.begin_path();
   cnv.line_width(2.5);
   cnv.add_circle(24, 8, 4);
   cnv.stroke();
}

void tauri(canvas& cnv)
{
   cnv.add_rect({{0, 0}, window_size});
   cnv.fill_style(tauri_bkd);
   cnv.fill();
   cnv.scale(10, 10);
   cnv.translate(9, 0);

   // Glow
   cnv.fill_style(tauri_bkd);
   cnv.stroke_style(tauri_bkd);
   cnv.shadow_style({-1, -1}, 10, colors::light_cyan);
   tauri_logo(cnv);

   // Shadow
   cnv.fill_style(tauri_bkd);
   cnv.stroke_style(tauri_bkd);
   cnv.shadow_style({5, 5}, 20, rgb(0, 0, 20));
   tauri_logo(cnv);

   // Gradient
   auto gr = canvas::linear_gradient{0, 0, 50, 50};
   gr.add_color_stop(0.0, colors::gold);
   gr.add_color_stop(1.0, colors::gold.opacity(0));
   cnv.fill_style(gr);
   cnv.stroke_style(gr);
   tauri_logo(cnv);
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

TEST_CASE("Composite2")
{
   image pm{window_size};
   {
      offscreen_image ctx{pm};
      canvas pm_cnv{ctx.context()};
      composite_ops2(pm_cnv);
   }
   compare_golden(pm, "composite_ops2");
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

TEST_CASE("Tauri")
{
   image pm{window_size};
   {
      offscreen_image ctx{pm};
      canvas pm_cnv{ctx.context()};
      tauri(pm_cnv);
   }
   compare_golden(pm, "tauri");
}
