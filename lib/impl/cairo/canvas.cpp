/*=============================================================================
   Copyright (c) 2016-2024 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <artist/canvas.hpp>
#include <artist/image.hpp>
#include "cairo_private.hpp"
#include <stack>
#include <cmath>
#include <functional>
#include <stdexcept>

namespace cycfi::artist
{
   ////////////////////////////////////////////////////////////////////////////
   // canvas_state — manages per-save/restore paint style and text alignment.
   // Styles are stored as deferred lambdas and applied just before each draw.
   class canvas::canvas_state
   {
   public:

      struct info
      {
         enum pattern_state { none_set, stroke_set, fill_set };

         std::function<void()>  stroke_style;
         std::function<void()>  fill_style;
         int                    align       = 0;
         pattern_state          pattern_set = none_set;
      };

      void  apply_fill_style();
      void  apply_stroke_style();

      using state_stack = std::stack<info>;

      info        _info;
      state_stack _stack;
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

   ////////////////////////////////////////////////////////////////////////////
   canvas::canvas(canvas_impl* context_)
    : _context{context_}
    , _state{std::make_unique<canvas_state>()}
   {
   }

   canvas::~canvas()
   {
   }

   ////////////////////////////////////////////////////////////////////////////
   // Transforms

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

   void canvas::skew(double sx, double sy)
   {
      cairo_matrix_t m;
      cairo_matrix_init(&m, 1.0, sy, sx, 1.0, 0.0, 0.0);
      cairo_transform(_context, &m);
   }

   point canvas::device_to_user(point p)
   {
      double x = p.x, y = p.y;
      cairo_device_to_user(_context, &x, &y);
      return {float(x), float(y)};
   }

   point canvas::user_to_device(point p)
   {
      double x = p.x, y = p.y;
      cairo_user_to_device(_context, &x, &y);
      return {float(x), float(y)};
   }

   // Cairo matrix: {xx=a, yx=b, xy=c, yy=d, x0=tx, y0=ty}
   affine_transform canvas::transform() const
   {
      cairo_matrix_t m;
      cairo_get_matrix(_context, &m);
      return affine_transform{m.xx, m.yx, m.xy, m.yy, m.x0, m.y0};
   }

   void canvas::transform(affine_transform const& mat)
   {
      cairo_matrix_t m{mat.a, mat.b, mat.c, mat.d, mat.tx, mat.ty};
      cairo_set_matrix(_context, &m);
   }

   void canvas::transform(double a, double b, double c, double d, double tx, double ty)
   {
      cairo_matrix_t m{a, b, c, d, tx, ty};
      cairo_set_matrix(_context, &m);
   }

   ////////////////////////////////////////////////////////////////////////////
   // Save / restore

   void canvas::save()
   {
      cairo_save(_context);
      _state->_stack.push(_state->_info);
   }

   void canvas::restore()
   {
      if (!_state->_stack.empty())
      {
         _state->_info = _state->_stack.top();
         _state->_stack.pop();
      }
      cairo_restore(_context);
   }

   ////////////////////////////////////////////////////////////////////////////
   // Paths

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

   void canvas::clip(path const& p)
   {
      if (!p.impl()) return;
      auto* cp = cairo_copy_path(p.impl()->ctx);
      if (cp)
      {
         cairo_append_path(_context, cp);
         cairo_path_destroy(cp);
      }
      // Apply the path's fill rule before clipping — Cairo clips with the
      // current fill rule, so odd-even paths need it set on the context.
      cairo_set_fill_rule(_context,
         p.impl()->fill_rule == path::fill_odd_even
            ? CAIRO_FILL_RULE_EVEN_ODD : CAIRO_FILL_RULE_WINDING);
      cairo_clip(_context);
   }

   rect canvas::clip_extent() const
   {
      double x1, y1, x2, y2;
      cairo_clip_extents(_context, &x1, &y1, &x2, &y2);
      return {float(x1), float(y1), float(x2), float(y2)};
   }

   bool canvas::point_in_path(point p) const
   {
      return cairo_in_fill(_context, p.x, p.y);
   }

   rect canvas::fill_extent() const
   {
      double x1, y1, x2, y2;
      cairo_fill_extents(_context, &x1, &y1, &x2, &y2);
      return {float(x1), float(y1), float(x2), float(y2)};
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
      auto mm = std::fabs(a1 * b2 - b1 * a2);

      if (mm < 1.0e-8)
      {
         line_to(p1);
         return;
      }

      auto dd = a1*a1 + b1*b1;
      auto cc = a2*a2 + b2*b2;
      auto tt = a1*a2 + b1*b2;
      auto k1 = radius * std::sqrt(dd) / mm;
      auto k2 = radius * std::sqrt(cc) / mm;
      auto j1 = k1 * tt / dd;
      auto j2 = k2 * tt / cc;
      auto cx = k1*b2 + k2*b1;
      auto cy = k1*a2 + k2*a1;
      auto px = b1*(k2+j1);
      auto py = a1*(k2+j1);
      auto qx = b2*(k1+j2);
      auto qy = a2*(k1+j2);
      auto start_angle = std::atan2(py - cy, px - cx);
      auto end_angle   = std::atan2(qy - cy, qx - cx);
      bool ccw = (b1*a2 > b2*a1);

      arc({float(cx + p1.x), float(cy + p1.y)},
          radius, float(start_angle), float(end_angle), ccw);
   }

   void canvas::arc(
      point p, float radius,
      float start_angle, float end_angle,
      bool ccw
   )
   {
      radius = std::max(radius, 0.0f);
      if (ccw)
         cairo_arc_negative(_context, p.x, p.y, radius, start_angle, end_angle);
      else
         cairo_arc(_context, p.x, p.y, radius, start_angle, end_angle);
   }

   void canvas::add_rect(rect const& r)
   {
      cairo_rectangle(_context, r.left, r.top, r.width(), r.height());
   }

   void canvas::add_path(path const& p)
   {
      if (!p.impl()) return;
      auto* cp = cairo_copy_path(p.impl()->ctx);
      if (cp)
      {
         cairo_append_path(_context, cp);
         cairo_path_destroy(cp);
      }
   }

   void canvas::clear_rect(rect const& r)
   {
      auto s = new_state();
      cairo_set_operator(_context, CAIRO_OPERATOR_CLEAR);
      cairo_rectangle(_context, r.left, r.top, r.width(), r.height());
      cairo_fill(_context);
   }

   void canvas::quadratic_curve_to(point cp, point end)
   {
      double x, y;
      cairo_get_current_point(_context, &x, &y);
      cairo_curve_to(_context,
         2.0/3.0 * cp.x + 1.0/3.0 * x,
         2.0/3.0 * cp.y + 1.0/3.0 * y,
         2.0/3.0 * cp.x + 1.0/3.0 * end.x,
         2.0/3.0 * cp.y + 1.0/3.0 * end.y,
         end.x, end.y
      );
   }

   void canvas::bezier_curve_to(point cp1, point cp2, point end)
   {
      cairo_curve_to(_context, cp1.x, cp1.y, cp2.x, cp2.y, end.x, end.y);
   }

   void canvas::add_round_rect_impl(rect const& r, float radius)
   {
      auto x = r.left, y = r.top, w = r.right, b = r.bottom;
      constexpr auto a = M_PI / 180.0;
      cairo_new_sub_path(_context);
      cairo_arc(_context, w-radius, y+radius, radius, -90*a,   0*a);
      cairo_arc(_context, w-radius, b-radius, radius,   0*a,  90*a);
      cairo_arc(_context, x+radius, b-radius, radius,  90*a, 180*a);
      cairo_arc(_context, x+radius, y+radius, radius, 180*a, 270*a);
      cairo_close_path(_context);
   }

   ////////////////////////////////////////////////////////////////////////////
   // Styles

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
         case butt:   cap = CAIRO_LINE_CAP_BUTT;   break;
         case round:  cap = CAIRO_LINE_CAP_ROUND;  break;
         case square: cap = CAIRO_LINE_CAP_SQUARE; break;
      }
      cairo_set_line_cap(_context, cap);
   }

   void canvas::line_join(join_enum join_)
   {
      cairo_line_join_t join = CAIRO_LINE_JOIN_MITER;
      switch (join_)
      {
         case bevel_join: join = CAIRO_LINE_JOIN_BEVEL; break;
         case round_join: join = CAIRO_LINE_JOIN_ROUND; break;
         case miter_join: join = CAIRO_LINE_JOIN_MITER; break;
      }
      cairo_set_line_join(_context, join);
   }

   void canvas::miter_limit(float limit)
   {
      cairo_set_miter_limit(_context, limit);
   }

   void canvas::shadow_style(point /*offset*/, float /*blur*/, color /*c*/)
   {
      // TODO(cairo): Shadow rendering requires compositing with a blurred copy.
      // Cairo has no native drop-shadow operation. Known limitation — no-op.
      // See Skia implementation (SkImageFilters::DropShadow) for expected behavior.
   }

   void canvas::global_composite_operation(composite_op_enum mode)
   {
      // Porter-Duff ops: directly supported by Cairo.
      // Extended blend modes (MULTIPLY … HSL_LUMINOSITY): require Cairo >= 1.10.
      // lighter: W3C Porter-Duff Plus (additive) — CAIRO_OPERATOR_ADD is correct.
      // darker: W3C PlusDarker = max(0, Cs+Cd-1). Cairo has no native equivalent;
      //   OPERATOR_DARKEN (channel-min) is used as a known approximation.
      cairo_operator_t op;
      switch (mode)
      {
         case source_over:      op = CAIRO_OPERATOR_OVER;            break;
         case source_atop:      op = CAIRO_OPERATOR_ATOP;            break;
         case source_in:        op = CAIRO_OPERATOR_IN;              break;
         case source_out:       op = CAIRO_OPERATOR_OUT;             break;
         case destination_over: op = CAIRO_OPERATOR_DEST_OVER;       break;
         case destination_atop: op = CAIRO_OPERATOR_DEST_ATOP;       break;
         case destination_in:   op = CAIRO_OPERATOR_DEST_IN;         break;
         case destination_out:  op = CAIRO_OPERATOR_DEST_OUT;        break;
         case lighter:          op = CAIRO_OPERATOR_ADD;             break;
         case darker:           op = CAIRO_OPERATOR_DARKEN;          break;
         case copy:             op = CAIRO_OPERATOR_SOURCE;          break;
         case xor_:             op = CAIRO_OPERATOR_XOR;             break;
         // Cairo >= 1.10 extended blend operators:
         case difference:       op = CAIRO_OPERATOR_DIFFERENCE;      break;
         case exclusion:        op = CAIRO_OPERATOR_EXCLUSION;       break;
         case multiply:         op = CAIRO_OPERATOR_MULTIPLY;        break;
         case screen:           op = CAIRO_OPERATOR_SCREEN;          break;
         case color_dodge:      op = CAIRO_OPERATOR_COLOR_DODGE;     break;
         case color_burn:       op = CAIRO_OPERATOR_COLOR_BURN;      break;
         case soft_light:       op = CAIRO_OPERATOR_SOFT_LIGHT;      break;
         case hard_light:       op = CAIRO_OPERATOR_HARD_LIGHT;      break;
         // Cairo >= 1.10 HSL blend operators:
         case hue:              op = CAIRO_OPERATOR_HSL_HUE;         break;
         case saturation:       op = CAIRO_OPERATOR_HSL_SATURATION;  break;
         case color_op:         op = CAIRO_OPERATOR_HSL_COLOR;       break;
         case luminosity:       op = CAIRO_OPERATOR_HSL_LUMINOSITY;  break;
         default:
            throw std::runtime_error{
               "artist cairo backend: unhandled composite_op_enum value"};
      }
      cairo_set_operator(_context, op);
   }

   ////////////////////////////////////////////////////////////////////////////
   // Fill rule

   void canvas::fill_rule(path::fill_rule_enum rule)
   {
      cairo_set_fill_rule(_context,
         rule == path::fill_winding ? CAIRO_FILL_RULE_WINDING : CAIRO_FILL_RULE_EVEN_ODD);
   }

   ////////////////////////////////////////////////////////////////////////////
   // Gradients
   // Pattern lifetime: cairo_set_source takes its own reference, so we call
   // cairo_pattern_destroy immediately after to release the caller's reference.

   namespace
   {
      cairo_pattern_t* make_linear_pattern(canvas::linear_gradient const& gr)
      {
         auto* pat = cairo_pattern_create_linear(
            gr.start.x, gr.start.y, gr.end.x, gr.end.y);
         for (auto const& cs : gr.color_space)
            cairo_pattern_add_color_stop_rgba(
               pat, cs.offset,
               cs.color.red, cs.color.green, cs.color.blue, cs.color.alpha);
         return pat;
      }

      cairo_pattern_t* make_radial_pattern(canvas::radial_gradient const& gr)
      {
         auto* pat = cairo_pattern_create_radial(
            gr.c1.x, gr.c1.y, gr.c1_radius,
            gr.c2.x, gr.c2.y, gr.c2_radius);
         for (auto const& cs : gr.color_space)
            cairo_pattern_add_color_stop_rgba(
               pat, cs.offset,
               cs.color.red, cs.color.green, cs.color.blue, cs.color.alpha);
         return pat;
      }
   }

   void canvas::fill_style(linear_gradient const& gr)
   {
      _state->_info.fill_style = [this, gr]()
      {
         auto* pat = make_linear_pattern(gr);
         cairo_set_source(_context, pat);
         cairo_pattern_destroy(pat);
      };
      if (_state->_info.pattern_set == _state->_info.fill_set)
         _state->_info.pattern_set = _state->_info.none_set;
   }

   void canvas::fill_style(radial_gradient const& gr)
   {
      _state->_info.fill_style = [this, gr]()
      {
         auto* pat = make_radial_pattern(gr);
         cairo_set_source(_context, pat);
         cairo_pattern_destroy(pat);
      };
      if (_state->_info.pattern_set == _state->_info.fill_set)
         _state->_info.pattern_set = _state->_info.none_set;
   }

   void canvas::stroke_style(linear_gradient const& gr)
   {
      _state->_info.stroke_style = [this, gr]()
      {
         auto* pat = make_linear_pattern(gr);
         cairo_set_source(_context, pat);
         cairo_pattern_destroy(pat);
      };
      if (_state->_info.pattern_set == _state->_info.stroke_set)
         _state->_info.pattern_set = _state->_info.none_set;
   }

   void canvas::stroke_style(radial_gradient const& gr)
   {
      _state->_info.stroke_style = [this, gr]()
      {
         auto* pat = make_radial_pattern(gr);
         cairo_set_source(_context, pat);
         cairo_pattern_destroy(pat);
      };
      if (_state->_info.pattern_set == _state->_info.stroke_set)
         _state->_info.pattern_set = _state->_info.none_set;
   }

   ////////////////////////////////////////////////////////////////////////////
   // Font / text — Stage 5: FreeType/Fontconfig-backed font support.

   void canvas::font(class font const& font_)
   {
      if (font_.impl() && font_.impl()->_scaled_font)
         cairo_set_scaled_font(_context, font_.impl()->_scaled_font);
   }

   namespace
   {
      point get_text_start(cairo_t* ctx, point p, int align, char const* utf8)
      {
         cairo_text_extents_t extents;
         cairo_text_extents(ctx, utf8, &extents);

         cairo_font_extents_t font_extents;
         cairo_scaled_font_extents(cairo_get_scaled_font(ctx), &font_extents);

         switch (align & 0x3)
         {
            case canvas::text_halign::right:
               p.x -= float(extents.width);
               break;
            case canvas::text_halign::center:
               p.x -= float(extents.width) / 2;
               break;
            default:
               break;
         }

         switch (align & 0x1C)
         {
            case canvas::text_valign::top:
               p.y += float(font_extents.ascent);
               break;
            case canvas::text_valign::middle:
               p.y += float(font_extents.ascent) / 2 - float(font_extents.descent) / 2;
               break;
            case canvas::text_valign::bottom:
               p.y -= float(font_extents.descent);
               break;
            default:
               break;
         }

         return p;
      }
   }

   void canvas::fill_text(std::string_view utf8, point p)
   {
      auto str = std::string{utf8.data(), utf8.size()};
      _state->apply_fill_style();
      p = get_text_start(_context, p, _state->_info.align, str.c_str());
      cairo_move_to(_context, p.x, p.y);
      cairo_show_text(_context, str.c_str());
   }

   void canvas::stroke_text(std::string_view utf8, point p)
   {
      auto str = std::string{utf8.data(), utf8.size()};
      _state->apply_stroke_style();
      p = get_text_start(_context, p, _state->_info.align, str.c_str());
      cairo_move_to(_context, p.x, p.y);
      cairo_text_path(_context, str.c_str());
      stroke();
   }

   canvas::text_metrics canvas::measure_text(std::string_view utf8)
   {
      auto str = std::string{utf8.data(), utf8.size()};
      cairo_text_extents_t extents;
      cairo_scaled_font_text_extents(cairo_get_scaled_font(_context),
         str.c_str(), &extents);

      cairo_font_extents_t font_extents;
      cairo_scaled_font_extents(cairo_get_scaled_font(_context), &font_extents);

      float ascent  = float(font_extents.ascent);
      float descent = float(font_extents.descent);
      float leading = float(font_extents.height) - ascent - descent;
      if (leading < 0) leading = 0;

      return {
         ascent,
         descent,
         leading,
         // size.x = advance width; size.y = line height (ascent + descent)
         {float(extents.x_advance), ascent + descent}
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

   ////////////////////////////////////////////////////////////////////////////
   // Images

   void canvas::draw(image const& pic, rect const& src, rect const& dest)
   {
      if (!pic.impl() || !pic.impl()->surface) return;
      auto s = new_state();
      auto w = dest.width();
      auto h = dest.height();
      translate(dest.top_left());
      auto sx = w / src.width();
      auto sy = h / src.height();
      scale({sx, sy});
      // Clip to the destination rect before painting. Cairo's unbounded operators
      // (IN, OUT, SOURCE, DEST_IN, XOR, etc.) affect the entire clip region, not
      // just the filled path. Without this clip they would clear the whole surface.
      cairo_rectangle(_context, 0, 0, src.width(), src.height());
      cairo_clip(_context);
      cairo_set_source_surface(_context, pic.impl()->surface, -src.left, -src.top);
      cairo_paint(_context);
   }
}
