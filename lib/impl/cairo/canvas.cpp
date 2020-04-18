/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <artist/canvas.hpp>

namespace cycfi::artist
{
   class canvas::canvas_state
   {
   };

   canvas::canvas(canvas_impl_ptr context_)
    : _context{ context_ }
    , _state{ std::make_unique<canvas_state>() }
   {
   }

   canvas::~canvas()
   {
   }

   void canvas::pre_scale(point p)
   {
   }

   void canvas::translate(point p)
   {
   }

   void canvas::rotate(float rad)
   {
   }

   void canvas::scale(point p)
   {
   }

   void canvas::save()
   {
   }

   void canvas::restore()
   {
   }

   void canvas::begin_path()
   {
   }

   void canvas::close_path()
   {
   }

   void canvas::fill()
   {
   }

   void canvas::fill_preserve()
   {
   }

   void canvas::stroke()
   {
   }

   void canvas::stroke_preserve()
   {
   }

   void canvas::clip()
   {
   }

   void canvas::move_to(point p)
   {
   }

   void canvas::line_to(point p)
   {
   }

   void canvas::arc_to(point p1, point p2, float radius)
   {
   }

   void canvas::arc(
      point p, float radius,
      float start_angle, float end_angle,
      bool ccw
   )
   {
   }

   void canvas::rect(struct rect r)
   {
   }

   void canvas::round_rect(struct rect r, float radius)
   {
   }

   void canvas::circle(struct circle c)
   {
   }

   void canvas::path(class path const& p)
   {
   }

   void canvas::quadratic_curve_to(point cp, point end)
   {
   }

   void canvas::bezier_curve_to(point cp1, point cp2, point end)
   {
   }

   void canvas::fill_style(color c)
   {
   }

   void canvas::stroke_style(color c)
   {
   }

   void canvas::line_width(float w)
   {
   }

   void canvas::line_cap(line_cap_enum cap_)
   {
   }

   void canvas::line_join(join_enum join_)
   {
   }

   void canvas::miter_limit(float limit)
   {
   }

   void canvas::shadow_style(point offset, float blur, color c)
   {
   }

   void canvas::global_composite_operation(composite_op_enum mode)
   {
   }

   void canvas::fill_style(linear_gradient const& gr)
   {
   }

   void canvas::fill_style(radial_gradient const& gr)
   {
   }

   void canvas::stroke_style(linear_gradient const& gr)
   {
   }

   void canvas::stroke_style(radial_gradient const& gr)
   {
   }

   void canvas::font(class font const& font_)
   {
   }

   void canvas::fill_text(std::string_view utf8, point p)
   {
   }

   void canvas::stroke_text(std::string_view utf8, point p)
   {
   }

   canvas::text_metrics canvas::measure_text(std::string_view utf8)
   {
      return {};
   }

   void canvas::text_align(int align)
   {
   }

   void canvas::text_align(text_halign align)
   {
   }

   void canvas::text_baseline(text_valign align)
   {
   }

   void canvas::draw(image const& pic, struct rect src, struct rect dest)
   {
   }
}
