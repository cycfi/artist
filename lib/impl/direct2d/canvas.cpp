/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <infra/support.hpp>
#include <artist/canvas.hpp>
#include <canvas_impl.hpp>
#include <path_impl.hpp>
#include <stack>

namespace cycfi::artist
{
   class canvas::canvas_state : public canvas_state_impl
   {
   public:

      virtual void      update(d2d_canvas& cnv);
      virtual void      discard();

      artist::path&     path();
      void              fill_paint(color c, d2d_canvas& cnv);
      void              stroke_paint(color c, d2d_canvas& cnv);
      void              line_width(float w);
      void              fill(d2d_canvas& cnv, bool preserve);
      void              stroke(d2d_canvas& cnv, bool preserve);

   private:

      artist::path      _path;               // for now
      color             _fill_color;         // for now
      d2d_paint*        _fill_paint;         // for now
      color             _stroke_color;       // for now
      d2d_paint*        _stroke_paint;       // for now
      float             _line_width = 1;     // for now
   };

   void canvas::canvas_state::update(d2d_canvas& cnv)
   {
      if (!_fill_paint)
         _fill_paint = make_paint(_fill_color, cnv);
      if (!_stroke_paint)
         _stroke_paint = make_paint(_stroke_color, cnv);
   }

   void canvas::canvas_state::discard()
   {
      release(_fill_paint);
      release(_stroke_paint);
   }

   artist::path& canvas::canvas_state::path()
   {
      return _path;
   }

   void canvas::canvas_state::fill_paint(color c, d2d_canvas& cnv)
   {
      if (c != _fill_color)
      {
         _fill_color = c;
         release(_fill_paint);
         _fill_paint = make_paint(_fill_color, cnv);
      }
   }

   void canvas::canvas_state::stroke_paint(color c, d2d_canvas& cnv)
   {
      if (c != _stroke_color)
      {
         _stroke_color = c;
         release(_stroke_paint);
         _stroke_paint = make_paint(_stroke_color, cnv);
      }
   }

   void canvas::canvas_state::line_width(float w)
   {
      _line_width = w;
   }

   void canvas::canvas_state::fill(d2d_canvas& cnv, bool preserve)
   {
      // for now
      if (!_path.impl()->empty())
      {
         cnv.FillGeometry(
            _path.impl()->compute_fill()
          , _fill_paint
          , nullptr
         );
         if (!preserve)
            _path.impl()->clear();
      }
   }

   void canvas::canvas_state::stroke(d2d_canvas& cnv, bool preserve)
   {
      if (!_path.impl()->empty())
      {
         for (auto p : *_path.impl())
            cnv.DrawGeometry(p, _stroke_paint, _line_width, nullptr);
         if (!preserve)
            _path.impl()->clear();
      }
   }

   canvas::canvas(canvas_impl_ptr context_)
    : _context{ context_ }
    , _state{ std::make_unique<canvas_state>() }
   {
      _context->state(_state.get());
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
      _state->fill(*_context->canvas(), false);
   }

   void canvas::fill_preserve()
   {
      _state->fill(*_context->canvas(), true);
   }

   void canvas::stroke()
   {
      _state->stroke(*_context->canvas(), false);
   }

   void canvas::stroke_preserve()
   {
      _state->stroke(*_context->canvas(), true);
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
      _state->path().add(r);
   }

   void canvas::round_rect(struct rect r, float radius)
   {
      _state->path().add(r, radius);
   }

   void canvas::circle(struct circle c)
   {
      _state->path().add(c);
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
      _state->fill_paint(c, *_context->canvas());
   }

   void canvas::stroke_style(color c)
   {
      _state->stroke_paint(c, *_context->canvas());
   }

   void canvas::line_width(float w)
   {
      _state->line_width(w);
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

   void canvas::fill_rule(path::fill_rule_enum rule)
   {
      _state->path().fill_rule(rule);
   }
}
