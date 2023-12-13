/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <artist/canvas.hpp>
#include <cairo.h>
#include <stack>

namespace cycfi::artist
{
   class canvas::canvas_state
   {
   public:

      struct info
      {
         enum pattern_state { none_set, stroke_set, fill_set };

         std::function<void()>   stroke_style;
         std::function<void()>   fill_style;
         int                     align = 0;
         pattern_state           pattern_set = none_set;
      };

      void              apply_fill_style();
      void              apply_stroke_style();
      float             pre_scale = 1.0f;

      using state_stack = std::stack<info>;

      info              _info;
      state_stack       _stack;
   };

   inline void canvas::canvas_state::apply_fill_style()
   {
      if (_info.pattern_set != _info.fill_set && _info.fill_style)
      {
         _info.fill_style();
         _info.pattern_set = _info.fill_set;
      }
   }

   inline void canvas::canvas_state::apply_stroke_style()
   {
      if (_info.pattern_set != _info.stroke_set && _info.stroke_style)
      {
         _info.stroke_style();
         _info.pattern_set = _info.stroke_set;
      }
   }

   canvas::canvas(canvas_impl* context_)
    : _context{ context_ }
    , _state{ std::make_unique<canvas_state>() }
   {
   }

   canvas::~canvas()
   {
   }

   void canvas::pre_scale(float sc)
   {
      scale({sc,sc});
      _state->pre_scale = sc;
   }

   void canvas::translate(point p)
   {
      cairo_translate(_context, p.x, p.y);
   }

   void canvas::rotate(float rad)
   {
      cairo_rotate(_context, rad);
   }

   void canvas::scale(point p)
   {
      cairo_scale(_context, p.x, p.y);
   }

   point canvas::device_to_user(point p) {
       double x = p.x * _state->pre_scale;
       double y = p.y * _state->pre_scale;
       cairo_device_to_user_distance(_context, &x, &y);
       return {float(x),float(y)};
   }

   point canvas::user_to_device(point p) {
       double x = p.x;
       double y = p.y;
       cairo_user_to_device_distance(_context, &x, &y);
       return {float(x / _state->pre_scale),float(y / _state->pre_scale)};
   }

   void canvas::save()
   {
      cairo_save(_context);
      _state->_stack.push(_state->_info);
   }

   void canvas::restore()
   {
      _state->_info = _state->_stack.top();
      _state->_stack.pop();
      cairo_restore(_context);
   }

   void canvas::begin_path()
   {
      cairo_new_path(_context);
   }

   void canvas::close_path()
   {
      cairo_close_path(_context);
   }

   void canvas::fill_rule(path::fill_rule_enum rule) {
      //TODO
   }

   void canvas::fill()
   {
      _state->apply_fill_style();
      cairo_fill(_context);
   }

   void canvas::fill_preserve()
   {
      _state->apply_fill_style();
      cairo_fill_preserve(_context);
   }

   void canvas::stroke()
   {
      _state->apply_stroke_style();
      cairo_stroke(_context);
   }

   void canvas::stroke_preserve()
   {
      _state->apply_stroke_style();
      cairo_stroke_preserve(_context);
   }

   void canvas::clip()
   {
      cairo_clip(_context);
   }

   bool canvas::point_in_path(point p) const {
      // not sure how to implement this on cairo
      return false;
   }

   void canvas::move_to(point p)
   {
      cairo_move_to(_context, p.x, p.y);
   }

   void canvas::line_to(point p)
   {
      cairo_line_to(_context, p.x, p.y);
   }

   void canvas::arc_to(point p1, point p2, float radius)
   {
     // Adapted from http://code.google.com/p/fxcanvas/

      if (radius == 0)
      {
         line_to(p1);
         return;
      }

      double cpx, cpy;
      cairo_get_current_point(_context, &cpx, &cpy);

      auto a1 = cpy - p1.y;
      auto b1 = cpx - p1.x;
      auto a2 = p2.y - p1.y;
      auto b2 = p2.x - p1.x;
      auto mm = fabs(a1 * b2 - b1 * a2);

      if (mm < 1.0e-8)
      {
         line_to(p1);
         return;
      }

      auto dd = a1 * a1 + b1 * b1;
      auto cc = a2 * a2 + b2 * b2;
      auto tt = a1 * a2 + b1 * b2;
      auto k1 = radius * std::sqrtf(dd) / mm;
      auto k2 = radius * std::sqrtf(cc) / mm;
      auto j1 = k1 * tt / dd;
      auto j2 = k2 * tt / cc;
      auto cx = k1 * b2 + k2 * b1;
      auto cy = k1 * a2 + k2 * a1;
      auto px = b1 * (k2 + j1);
      auto py = a1 * (k2 + j1);
      auto qx = b2 * (k1 + j2);
      auto qy = a2 * (k1 + j2);
      auto start_angle = std::atan2f(py - cy, px - cx);
      auto end_angle = std::atan2f(qy - cy, qx - cx);

      arc(
         { float(cx + p1.x), float(cy + p1.y) }
       , radius, start_angle, end_angle
       , (b1 * a2 > b2 * a1)
      );
   }

   void canvas::arc(
      point p, float radius,
      float start_angle, float end_angle,
      bool ccw
   )
   {
      if (ccw)
         cairo_arc_negative(_context, p.x, p.y, radius, start_angle, end_angle);
      else
         cairo_arc(_context, p.x, p.y, radius, start_angle, end_angle);
   }

   void canvas::add_rect(rect const& r)
   {
      cairo_rectangle(_context, r.left, r.top, r.width(), r.height());
   }

   void canvas::add_round_rect_impl(rect const& bounds, float radius)
   {
      auto x = bounds.left;
      auto y = bounds.top;
      auto r = bounds.right;
      auto b = bounds.bottom;
      auto const a = M_PI/180.0;
      radius = std::min(radius, std::min(bounds.width(), bounds.height()));

      cairo_new_sub_path(_context);
      cairo_arc(_context, r-radius, y+radius, radius, -90*a, 0*a);
      cairo_arc(_context, r-radius, b-radius, radius, 0*a, 90*a);
      cairo_arc(_context, x+radius, b-radius, radius, 90*a, 180*a);
      cairo_arc(_context, x+radius, y+radius, radius, 180*a, 270*a);
      cairo_close_path(_context);
   }

   void canvas::add_path(path const& p)
   {
   }

   void canvas::quadratic_curve_to(point cp, point end)
   {
      double x, y;
      cairo_get_current_point(_context, &x, &y);
      cairo_curve_to(_context,
         2.0 / 3.0 * cp.x + 1.0 / 3.0 * x,
         2.0 / 3.0 * cp.y + 1.0 / 3.0 * y,
         2.0 / 3.0 * cp.x + 1.0 / 3.0 * end.x,
         2.0 / 3.0 * cp.y + 1.0 / 3.0 * end.y,
         end.x, end.y
      );
   }

   void canvas::bezier_curve_to(point cp1, point cp2, point end)
   {
      cairo_curve_to(_context, cp1.x, cp1.y, cp2.x, cp2.y, end.x, end.y);
   }

   void canvas::fill_style(color c)
   {
      _state->_info.fill_style = [this, c]()
      {
         cairo_set_source_rgba(_context, c.red, c.green, c.blue, c.alpha);
      };
      if (_state->_info.pattern_set == _state->_info.fill_set)
         _state->_info.pattern_set = _state->_info.none_set;
   }

   void canvas::stroke_style(color c)
   {
      _state->_info.stroke_style = [this, c]()
      {
         cairo_set_source_rgba(_context, c.red, c.green, c.blue, c.alpha);
      };
      if (_state->_info.pattern_set == _state->_info.stroke_set)
         _state->_info.pattern_set = _state->_info.none_set;
   }

   void canvas::line_width(float w)
   {
      cairo_set_line_width(_context, w);
   }

   void canvas::line_cap(line_cap_enum cap_)
   {
      cairo_line_cap_t cap = CAIRO_LINE_CAP_BUTT;
      switch (cap_)
      {
         case butt:     cap = CAIRO_LINE_CAP_BUTT; break;
         case round:    cap = CAIRO_LINE_CAP_ROUND; break;
         case square:   cap = CAIRO_LINE_CAP_SQUARE; break;
      }
      cairo_set_line_cap(_context, cap);
   }

   void canvas::line_join(join_enum join_)
   {
      cairo_line_join_t join = CAIRO_LINE_JOIN_MITER;
      switch (join_)
      {
         case bevel_join:  join = CAIRO_LINE_JOIN_BEVEL; break;
         case round_join:  join = CAIRO_LINE_JOIN_ROUND; break;
         case miter_join:  join = CAIRO_LINE_JOIN_MITER; break;
      }
      cairo_set_line_join(_context, join);
   }

   void canvas::miter_limit(float limit)
   {
      cairo_set_miter_limit(_context, limit);
   }

   void canvas::shadow_style(point offset, float blur, color c)
   {
   }

   void canvas::global_composite_operation(composite_op_enum mode)
   {
   }

   void canvas::fill_style(linear_gradient const& gr)
   {
      _state->_info.fill_style = [this, gr]()
      {
         cairo_pattern_t* pat =
            cairo_pattern_create_linear(
               gr.start.x, gr.start.y, gr.end.x, gr.end.y
            );

         for (auto cs : gr.color_space)
         {
            cairo_pattern_add_color_stop_rgba(
               pat, cs.offset,
               cs.color.red, cs.color.green, cs.color.blue, cs.color.alpha
            );
         }
         cairo_set_source(_context, pat);
      };
      if (_state->_info.pattern_set == _state->_info.fill_set)
         _state->_info.pattern_set = _state->_info.none_set;
   }

   void canvas::fill_style(radial_gradient const& gr)
   {
      _state->_info.fill_style = [this, gr]()
      {
         cairo_pattern_t* pat =
            cairo_pattern_create_radial(
               gr.c1.x, gr.c1.y, gr.c1_radius,
               gr.c2.x, gr.c2.y, gr.c2_radius
            );

         for (auto cs : gr.color_space)
         {
            cairo_pattern_add_color_stop_rgba(
               pat, cs.offset,
               cs.color.red, cs.color.green, cs.color.blue, cs.color.alpha
            );
         }
         cairo_set_source(_context, pat);
      };
      if (_state->_info.pattern_set == _state->_info.fill_set)
         _state->_info.pattern_set = _state->_info.none_set;
   }

   void canvas::stroke_style(linear_gradient const& gr)
   {
      _state->_info.stroke_style = [this, gr]()
      {
         cairo_pattern_t* pat =
            cairo_pattern_create_linear(
               gr.start.x, gr.start.y, gr.end.x, gr.end.y
            );

         for (auto cs : gr.color_space)
         {
            cairo_pattern_add_color_stop_rgba(
               pat, cs.offset,
               cs.color.red, cs.color.green, cs.color.blue, cs.color.alpha
            );
         }
         cairo_set_source(_context, pat);
      };
      if (_state->_info.pattern_set == _state->_info.stroke_set)
         _state->_info.pattern_set = _state->_info.none_set;
   }

   void canvas::stroke_style(radial_gradient const& gr)
   {
      _state->_info.stroke_style = [this, gr]()
      {
         cairo_pattern_t* pat =
            cairo_pattern_create_radial(
               gr.c1.x, gr.c1.y, gr.c1_radius,
               gr.c2.x, gr.c2.y, gr.c2_radius
            );

         for (auto cs : gr.color_space)
         {
            cairo_pattern_add_color_stop_rgba(
               pat, cs.offset,
               cs.color.red, cs.color.green, cs.color.blue, cs.color.alpha
            );
         }
         cairo_set_source(_context, pat);
      };
      if (_state->_info.pattern_set == _state->_info.stroke_set)
         _state->_info.pattern_set = _state->_info.none_set;
   }

   void canvas::font(class font const& font_)
   {
   }

   namespace
   {
      point get_text_start(cairo_t* _context, point p, int align, char const* utf8)
      {
         cairo_text_extents_t extents;
         cairo_text_extents(_context, utf8, &extents);

         cairo_font_extents_t font_extents;
         cairo_scaled_font_extents(cairo_get_scaled_font(_context), &font_extents);

         switch (align & 0x3)
         {
            case canvas::text_halign::right:
               p.x -= extents.width;
               break;
            case canvas::text_halign::center:
               p.x -= extents.width/2;
               break;
            default:
               break;
         }

         switch (align & 0x1C)
         {
            case canvas::text_valign::top:
               p.y += font_extents.ascent;
               break;

            case canvas::text_valign::middle:
               p.y += font_extents.ascent/2 - font_extents.descent/2;
               break;

            case canvas::text_valign::bottom:
               p.y -= font_extents.descent;
               break;

            default:
               break;
         }

         return p;
      }
   }

   void canvas::fill_text(std::string_view utf8, point p)
   {
      auto str = std::string{ utf8.data(), utf8.size() };
      _state->apply_fill_style();
      p = get_text_start(_context, p, _state->_info.align, str.c_str());
      cairo_move_to(_context, p.x, p.y);
      cairo_show_text(_context, str.c_str());
   }

   void canvas::stroke_text(std::string_view utf8, point p)
   {
      auto str = std::string{ utf8.data(), utf8.size() };
      _state->apply_stroke_style();
      p = get_text_start(_context, p, _state->_info.align, str.c_str());
      cairo_move_to(_context, p.x, p.y);
      cairo_text_path(_context, str.c_str());
      stroke();
   }

   canvas::text_metrics canvas::measure_text(std::string_view utf8)
   {
      auto str = std::string{ utf8.data(), utf8.size() };
      cairo_text_extents_t extents;
      cairo_text_extents(_context, str.c_str(), &extents);

      cairo_font_extents_t font_extents;
      cairo_scaled_font_extents(cairo_get_scaled_font(_context), &font_extents);

      return {
         /*ascent=*/    float(font_extents.ascent),
         /*descent=*/   float(font_extents.descent),
         /*leading=*/   float(font_extents.height-(font_extents.ascent+font_extents.descent)),
         /*size=*/      { float(extents.width), float(extents.height) }
      };
   }

   void canvas::text_align(int align)
   {
      _state->_info.align = align;
   }

   void canvas::text_align(text_halign align)
   {
      _state->_info.align |= align;
   }

   void canvas::text_baseline(text_valign align)
   {
      _state->_info.align |= align;
   }

   void canvas::draw(image const& pic, rect const& src, rect const& dest)
   {
      auto  state = new_state();
      auto  w = dest.width();
      auto  h = dest.height();
      translate(dest.top_left());
      auto scale_ = point{ w/src.width(), h/src.height() };
      scale(scale_);
      cairo_set_source_surface(_context, pic.impl(), -src.left, -src.top);
      add_rect({ 0, 0, w/scale_.x, h/scale_.y });
      cairo_fill(_context);
   }
}
