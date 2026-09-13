/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]

   The recording canvas. Nothing is rasterized: each drawing operation is
   resolved to its extent in surface space, clipped, and added to the journal
   of the image being drawn into. Transforms, the state stack and the clip are
   tracked exactly as a drawing backend tracks them, so the queries (extents,
   hit tests, coordinate conversion) answer as the other backends do. The clip
   is kept as a rectangle: a clip to a rotated or curved path records its
   bounding box.
=============================================================================*/
#include "recording_impl.hpp"
#include <infra/utf8_utils.hpp>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <stack>

namespace cycfi::artist
{
   using recording::op;

   ////////////////////////////////////////////////////////////////////////////
   // canvas_state
   ////////////////////////////////////////////////////////////////////////////
   class canvas::canvas_state
   {
   public:

      struct paint
      {
         color                      c = colors::black;
         bool                       gradient = false;
      };

      struct info
      {
         affine_transform           matrix;
         paint                      fill;
         paint                      stroke;
         float                      line_width = 1;
         canvas::line_cap_enum      cap = canvas::butt;
         canvas::join_enum          join = canvas::miter_join;
         float                      miter = 10;
         point                      shadow_offset = {};
         float                      shadow_blur = 0;
         color                      shadow_color = colors::black;
         artist::font               font_;
         int                        align = canvas::baseline;
         canvas::composite_op_enum  composite = canvas::source_over;
         rect                       clip;          // surface space
      };

      explicit                      canvas_state(canvas_impl* ctx_);

      info&                         cur()          { return _stack.top(); }
      info const&                   cur() const    { return _stack.top(); }

      void                          save()         { _stack.push(_stack.top()); }
      void                          restore();
      void                          matrix(affine_transform const& m);

      void                          record(
                                       op kind, rect geometry, paint const& p
                                     , float line_width, std::string text = {}
                                    );

      void                          record_text(op kind, std::string_view utf8, rect box, color c);

      recording::path_impl          path;

   private:

      canvas_impl*                  _ctx;
      std::stack<info>              _stack;
   };

   canvas::canvas_state::canvas_state(canvas_impl* ctx_)
    : _ctx{ctx_}
   {
      info i;
      if (_ctx)
         i.clip = {0, 0, _ctx->size.x, _ctx->size.y};
      _stack.push(i);
   }

   void canvas::canvas_state::restore()
   {
      if (_stack.size() > 1)
      {
         _stack.pop();
         path.set_transform(cur().matrix);   // the path is not part of the state
      }
   }

   void canvas::canvas_state::matrix(affine_transform const& m)
   {
      cur().matrix = m;
      path.set_transform(m);
   }

   void canvas::canvas_state::record(
      op kind, rect geometry, paint const& p, float line_width, std::string text)
   {
      auto const& c = cur();

      // A shadow widens where ink can land. Its offset is in surface space.
      rect ink = geometry;
      if (c.shadow_color.alpha > 0 &&
         (c.shadow_blur > 0 || c.shadow_offset.x != 0 || c.shadow_offset.y != 0))
      {
         auto shadow = geometry
            .move(c.shadow_offset.x, c.shadow_offset.y)
            .inset(-c.shadow_blur, -c.shadow_blur);
         ink = union_(geometry, shadow);
      }

      recording::command cmd;
      cmd.kind = kind;
      cmd.geometry = geometry;
      // The unbounded operators reach the whole clip: they clear the
      // destination outside what is drawn.
      bool const unbounded =
         c.composite == canvas::source_in || c.composite == canvas::source_out
         || c.composite == canvas::destination_in
         || c.composite == canvas::destination_atop
         || c.composite == canvas::copy;
      cmd.bounds = unbounded? c.clip : recording::intersect(ink, c.clip);
      cmd.visible = cmd.bounds.width() > 0 && cmd.bounds.height() > 0;
      cmd.paint = p.c;
      cmd.gradient = p.gradient;
      cmd.line_width = line_width;
      cmd.line_cap = c.cap;
      cmd.line_join = c.join;
      cmd.miter_limit = c.miter;
      cmd.shadow_offset = c.shadow_offset;
      cmd.shadow_blur = c.shadow_blur;
      cmd.shadow_color = c.shadow_color;
      cmd.composite = c.composite;
      cmd.text = std::move(text);

      if (_ctx && _ctx->out)
         _ctx->out->add(cmd);
   }

   void canvas::canvas_state::record_text(op kind, std::string_view utf8, rect box, color c)
   {
      record(
         kind, recording::transform_bounds(cur().matrix, box),
         paint{c, false}, 0, std::string{utf8}
      );
   }

   namespace
   {
      canvas::canvas_state::paint make_paint(canvas::gradient const& gr)
      {
         auto c = gr.color_space.empty()? colors::black : gr.color_space.front().color;
         return {c, true};
      }

      // How far a stroke reaches past its path, in surface units: half the
      // line width, scaled by the larger axis of the transform. Miter spikes
      // can reach further; the recorded extent does not include them.
      float stroke_reach(affine_transform const& m, float line_width)
      {
         auto sx = std::sqrt(m.a * m.a + m.b * m.b);
         auto sy = std::sqrt(m.c * m.c + m.d * m.d);
         return float(line_width / 2 * std::max(sx, sy));
      }

      // The box a line of text occupies, placed by the alignment the other
      // backends use: the low two bits align horizontally, the next three
      // vertically.
      rect text_box(artist::font const& f, int align, point p, float width)
      {
         auto m = f.metrics();
         float ascent = m.ascent;
         float descent = m.descent;

         switch (align & 0x3)
         {
            case canvas::center:    p.x -= width / 2;                break;
            case canvas::right:     p.x -= width;                    break;
            default:                                                 break;
         }
         switch (align & 0x1C)
         {
            case canvas::top:       p.y += ascent;                   break;
            case canvas::middle:    p.y += (ascent - descent) / 2;   break;
            case canvas::bottom:    p.y -= descent;                  break;
            default:                                                 break;
         }
         return {p.x, p.y - ascent, p.x + width, p.y + descent};
      }
   }

   ////////////////////////////////////////////////////////////////////////////
   // canvas
   ////////////////////////////////////////////////////////////////////////////
   canvas::canvas(canvas_impl* context_)
    : _context{context_}
    , _state{std::make_unique<canvas_state>(context_)}
   {
      if (_context)
      {
         _context->state = _state.get();
         _context->text_sink =
            [](void* state, op kind, std::string_view utf8, rect box, color c)
            {
               static_cast<canvas_state*>(state)->record_text(kind, utf8, box, c);
            };
      }
   }

   canvas::~canvas()
   {
      if (_context && _context->state == _state.get())
      {
         _context->state = nullptr;
         _context->text_sink = nullptr;
      }
   }

   ////////////////////////////////////////////////////////////////////////////
   // Transforms
   void canvas::translate(point p)
   {
      _state->matrix(_state->cur().matrix.translate(p.x, p.y));
   }

   void canvas::rotate(float rad)
   {
      _state->matrix(_state->cur().matrix.rotate(rad));
   }

   void canvas::scale(point p)
   {
      _state->matrix(_state->cur().matrix.scale(p.x, p.y));
   }

   void canvas::skew(double sx, double sy)
   {
      // sx and sy are angles, as for affine_transform::skew.
      _state->matrix(_state->cur().matrix.skew(sx, sy));
   }

   point canvas::device_to_user(point p)
   {
      return _state->cur().matrix.invert().apply(p);
   }

   point canvas::user_to_device(point p)
   {
      return _state->cur().matrix.apply(p);
   }

   affine_transform canvas::transform() const
   {
      return _state->cur().matrix;
   }

   void canvas::transform(affine_transform const& mat)
   {
      _state->matrix(mat);
   }

   void canvas::transform(double a, double b, double c, double d, double tx, double ty)
   {
      _state->matrix(affine_transform{a, b, c, d, tx, ty});
   }

   ////////////////////////////////////////////////////////////////////////////
   // Paths
   void canvas::begin_path()
   {
      _state->path.clear();
      _state->path.set_transform(_state->cur().matrix);
   }

   void canvas::close_path()
   {
      _state->path.close();
   }

   void canvas::fill()
   {
      if (_state->path.drawable())
         _state->record(op::fill, _state->path.bounds(), _state->cur().fill, 0);
      begin_path();
   }

   void canvas::fill_preserve()
   {
      if (_state->path.drawable())
         _state->record(op::fill, _state->path.bounds(), _state->cur().fill, 0);
   }

   void canvas::stroke()
   {
      stroke_preserve();
      begin_path();
   }

   void canvas::stroke_preserve()
   {
      if (!_state->path.drawable())
         return;
      auto const& c = _state->cur();
      auto reach = stroke_reach(c.matrix, c.line_width);
      _state->record(
         op::stroke, _state->path.bounds().inset(-reach, -reach),
         c.stroke, c.line_width
      );
   }

   void canvas::clip()
   {
      // The clip consumes the current path, and clipping to nothing clips
      // everything away.
      auto& c = _state->cur();
      c.clip = _state->path.empty()?
         rect{} : recording::intersect(c.clip, _state->path.bounds());
      begin_path();
   }

   void canvas::clip(path const& p)
   {
      // As on Quartz 2D and Skia: clip to p alone, leaving the current path.
      auto& c = _state->cur();
      if (!p.impl() || p.impl()->empty())
      {
         c.clip = {};
         return;
      }
      auto device = recording::transform_bounds(c.matrix, p.impl()->bounds());
      c.clip = recording::intersect(c.clip, device);
   }

   rect canvas::clip_extent() const
   {
      auto const& c = _state->cur();
      if (c.clip.width() <= 0 || c.clip.height() <= 0)
         return {};
      return recording::transform_bounds(c.matrix.invert(), c.clip);
   }

   bool canvas::point_in_path(point p) const
   {
      return _state->path.includes(_state->cur().matrix.apply(p));
   }

   rect canvas::fill_extent() const
   {
      if (_state->path.empty())
         return {};
      return recording::transform_bounds(
         _state->cur().matrix.invert(), _state->path.bounds());
   }

   void canvas::move_to(point p)
   {
      _state->path.move_to(p);
   }

   void canvas::line_to(point p)
   {
      _state->path.line_to(p);
   }

   void canvas::arc_to(point p1, point p2, float radius)
   {
      _state->path.arc_to(p1, p2, radius);
   }

   void canvas::arc(point p, float radius, float start_angle, float end_angle, bool ccw)
   {
      _state->path.arc(p, radius, start_angle, end_angle, ccw);
   }

   void canvas::add_rect(rect const& r)
   {
      _state->path.add_rect(r);
   }

   void canvas::add_round_rect_impl(rect const& r, float radius)
   {
      _state->path.add_round_rect(r, radius);
   }

   void canvas::add_path(path const& p)
   {
      if (p.impl())
         _state->path.append(*p.impl());
   }

   void canvas::clear_rect(rect const& r)
   {
      _state->record(
         op::clear, recording::transform_bounds(_state->cur().matrix, r),
         canvas_state::paint{colors::black.opacity(0), false}, 0
      );
   }

   void canvas::quadratic_curve_to(point cp, point end)
   {
      _state->path.quad_to(cp, end);
   }

   void canvas::bezier_curve_to(point cp1, point cp2, point end)
   {
      _state->path.cubic_to(cp1, cp2, end);
   }

   ////////////////////////////////////////////////////////////////////////////
   // Styles
   void canvas::fill_style(color c)
   {
      _state->cur().fill = {c, false};
   }

   void canvas::stroke_style(color c)
   {
      _state->cur().stroke = {c, false};
   }

   void canvas::line_width(float w)
   {
      // Zero, negative, infinite and NaN widths are ignored.
      if (!(w > 0) || !std::isfinite(w))
         return;
      _state->cur().line_width = w;
   }

   void canvas::line_cap(line_cap_enum cap)
   {
      _state->cur().cap = cap;
   }

   void canvas::line_join(join_enum join)
   {
      _state->cur().join = join;
   }

   void canvas::miter_limit(float limit)
   {
      // Zero, negative, infinite and NaN limits are ignored.
      if (!(limit > 0) || !std::isfinite(limit))
         return;
      _state->cur().miter = limit;
   }

   void canvas::shadow_style(point offset, float blur, color c)
   {
      auto& s = _state->cur();
      s.shadow_offset = offset;
      s.shadow_blur = blur;
      s.shadow_color = c;
   }

   void canvas::global_composite_operation(composite_op_enum mode)
   {
      _state->cur().composite = mode;
   }

   void canvas::fill_style(linear_gradient const& gr)
   {
      _state->cur().fill = make_paint(gr);
   }

   void canvas::fill_style(radial_gradient const& gr)
   {
      _state->cur().fill = make_paint(gr);
   }

   void canvas::stroke_style(linear_gradient const& gr)
   {
      _state->cur().stroke = make_paint(gr);
   }

   void canvas::stroke_style(radial_gradient const& gr)
   {
      _state->cur().stroke = make_paint(gr);
   }

   void canvas::fill_rule(path::fill_rule_enum rule)
   {
      _state->path.rule = rule;
   }

   ////////////////////////////////////////////////////////////////////////////
   // Text
   void canvas::font(class font const& font_)
   {
      _state->cur().font_ = font_;
   }

   void canvas::fill_text(std::string_view utf8, point p)
   {
      auto const& c = _state->cur();
      auto box = text_box(c.font_, c.align, p, recording::text_advance(c.font_, utf8));
      _state->record(
         op::fill_text, recording::transform_bounds(c.matrix, box),
         c.fill, 0, std::string{utf8}
      );
   }

   void canvas::stroke_text(std::string_view utf8, point p)
   {
      auto const& c = _state->cur();
      auto box = text_box(c.font_, c.align, p, recording::text_advance(c.font_, utf8));
      auto reach = stroke_reach(c.matrix, c.line_width);
      _state->record(
         op::stroke_text,
         recording::transform_bounds(c.matrix, box).inset(-reach, -reach),
         c.stroke, c.line_width, std::string{utf8}
      );
   }

   canvas::text_metrics canvas::measure_text(std::string_view utf8)
   {
      auto const& f = _state->cur().font_;
      auto m = f.metrics();
      return {
         m.ascent, m.descent, m.leading,
         {recording::text_advance(f, utf8), m.ascent + m.descent}
      };
   }

   void canvas::text_align(int align)
   {
      _state->cur().align = align;
   }

   void canvas::text_align(text_halign align)
   {
      _state->cur().align |= align;
   }

   void canvas::text_baseline(text_valign align)
   {
      _state->cur().align |= align;
   }

   ////////////////////////////////////////////////////////////////////////////
   // Pixmaps
   void canvas::draw(image const& /*pic*/, rect const& /*src*/, rect const& dest)
   {
      _state->record(
         op::image, recording::transform_bounds(_state->cur().matrix, dest),
         canvas_state::paint{}, 0
      );
   }

   ////////////////////////////////////////////////////////////////////////////
   // States
   void canvas::save()
   {
      _state->save();
   }

   void canvas::restore()
   {
      _state->restore();
   }
}

namespace cycfi::artist::recording
{
   ////////////////////////////////////////////////////////////////////////////
   // journal
   ////////////////////////////////////////////////////////////////////////////
   void journal::add(command const& cmd)
   {
      _commands.push_back(cmd);
   }

   std::size_t journal::count(op kind) const
   {
      return std::size_t(std::count_if(
         _commands.begin(), _commands.end(),
         [kind](command const& c){ return c.kind == kind; }));
   }

   rect journal::ink_extents() const
   {
      bool any = false;
      rect ink;
      for (auto const& c : _commands)
      {
         if (!c.visible)
            continue;
         ink = any? union_(ink, c.bounds) : c.bounds;
         any = true;
      }
      return any? ink : rect{};
   }

   std::size_t journal::clipped_away() const
   {
      return std::size_t(std::count_if(
         _commands.begin(), _commands.end(),
         [](command const& c){ return !c.visible; }));
   }

   std::string journal::str() const
   {
      auto name = [](op kind)
      {
         switch (kind)
         {
            case op::fill:          return "fill";
            case op::stroke:        return "stroke";
            case op::clear:         return "clear";
            case op::fill_text:     return "fill_text";
            case op::stroke_text:   return "stroke_text";
            case op::image:         return "image";
         }
         return "?";
      };
      auto byte = [](float v){ return int(std::lround(std::clamp(v, 0.0f, 1.0f) * 255)); };

      std::string s;
      char buf[160];
      for (auto const& c : _commands)
      {
         std::snprintf(buf, sizeof buf, "%s [%.2f %.2f %.2f %.2f] #%02x%02x%02x%02x",
            name(c.kind), c.geometry.left, c.geometry.top, c.geometry.right, c.geometry.bottom,
            byte(c.paint.red), byte(c.paint.green), byte(c.paint.blue), byte(c.paint.alpha));
         s += buf;
         if (c.gradient)
            s += " gradient";
         if (c.kind == op::stroke || c.kind == op::stroke_text)
         {
            std::snprintf(buf, sizeof buf, " width=%.2f cap=%d join=%d miter=%.2f",
               c.line_width, int(c.line_cap), int(c.line_join), c.miter_limit);
            s += buf;
         }
         if (c.shadow_color.alpha > 0 &&
            (c.shadow_blur > 0 || c.shadow_offset.x != 0 || c.shadow_offset.y != 0))
         {
            std::snprintf(buf, sizeof buf, " shadow=[%.2f %.2f %.2f #%02x%02x%02x%02x]",
               c.shadow_offset.x, c.shadow_offset.y, c.shadow_blur,
               byte(c.shadow_color.red), byte(c.shadow_color.green),
               byte(c.shadow_color.blue), byte(c.shadow_color.alpha));
            s += buf;
         }
         if (c.composite != canvas::source_over)
         {
            std::snprintf(buf, sizeof buf, " op=%d", int(c.composite));
            s += buf;
         }
         if (!c.visible)
            s += " clipped";
         if (!c.text.empty())
            s += " \"" + c.text + "\"";
         s += '\n';
      }
      return s;
   }

   void record_text(canvas& cnv, op kind, std::string_view utf8, rect box, color c)
   {
      auto* ctx = cnv.impl();
      if (ctx && ctx->state && ctx->text_sink)
         ctx->text_sink(ctx->state, kind, utf8, box, c);
   }
}
