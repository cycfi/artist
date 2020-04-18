/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <artist/canvas.hpp>
#include <cairo.h>

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
      assert(false); // unimplemented
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

   void canvas::rect(struct rect r)
   {
      cairo_rectangle(_context, r.left, r.top, r.width(), r.height());
   }

   void canvas::round_rect(struct rect bounds, float radius)
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

   void canvas::path(class path const& p)
   {
      assert(false); // unimplemented
   }

   void canvas::quadratic_curve_to(point cp, point end)
   {
      assert(false); // unimplemented
   }

   void canvas::bezier_curve_to(point cp1, point cp2, point end)
   {
      assert(false); // unimplemented
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
      assert(false); // unimplemented
   }

   void canvas::line_join(join_enum join_)
   {
      assert(false); // unimplemented
   }

   void canvas::miter_limit(float limit)
   {
      assert(false); // unimplemented
   }

   void canvas::shadow_style(point offset, float blur, color c)
   {
      assert(false); // unimplemented
   }

   void canvas::global_composite_operation(composite_op_enum mode)
   {
      assert(false); // unimplemented
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
      assert(false); // unimplemented
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

   void canvas::draw(image const& pic, struct rect src, struct rect dest)
   {
      assert(false); // unimplemented
   }
}
