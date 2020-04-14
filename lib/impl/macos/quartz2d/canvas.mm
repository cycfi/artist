/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <artist/canvas.hpp>
#include <Quartz/Quartz.h>
#include <stack>
#include <variant>
#include "osx_utils.hpp"

namespace cycfi::artist
{
   class canvas::canvas_state
   {
   public:

      using style = std::variant<
         color
       , canvas::linear_gradient
       , canvas::radial_gradient
      >;

      using mode_enum = canvas::composite_op_enum;
      using color_space = std::vector<color_stop>;

                        canvas_state();
                        canvas_state(canvas_state const&) = delete;
      canvas_state&     operator=(canvas_state const&) = delete;

      style const&      fill_style() const;
      void              fill_style(style const& style);
      void              fill_style(style const& style, color_space const& space);
      CGGradientRef     fill_gradient() const;

      style const&      stroke_style() const;
      void              stroke_style(style const& style);
      void              stroke_style(style const& style, color_space const& space);
      CGGradientRef     stroke_gradient() const;

      class font const& font() const;
      void              font(class font const& font_);

      int               text_align() const;
      void              text_align(int align);

      mode_enum         mode() const;
      void              mode(mode_enum mode_);

      using fill_rule_enum = path::fill_rule_enum;

      fill_rule_enum    fill_rule() const                { return _fill_rule; }
      void              fill_rule(fill_rule_enum rule)   { _fill_rule = rule; }

      void              save();
      void              restore();


   private:

      struct state_info
      {
         style          _fill_style       = colors::black;
         CGGradientRef  _fill_gradient    = nullptr;
         style          _stroke_style     = colors::black;
         CGGradientRef  _stroke_gradient  = nullptr;
         class font     _font             = font_descr{ "Helvetica Neue", 12 };
         int            _text_align       = canvas::baseline;
         mode_enum      _mode             = source_over;
      };

      using state_info_ptr = std::unique_ptr<state_info>;
      using state_info_stack = std::stack<state_info_ptr>;

      state_info*       current() { return _stack.top().get(); }
      state_info const* current() const { return _stack.top().get(); }

      state_info_stack  _stack;
      fill_rule_enum    _fill_rule = fill_rule_enum::fill_winding;
   };

   namespace
   {
      void make_gradient(std::vector<canvas::color_stop> const& space, CGGradientRef& gradient)
      {
         auto nspaces = space.size();
         CGFloat locations[nspaces];
         CGFloat components[nspaces * 4];
         for (size_t i = 0; i != nspaces; ++i)
         {
            locations[i] = space[i].offset;
            auto* cp = &components[i*4];
            *cp++ = space[i].color.red;
            *cp++ = space[i].color.green;
            *cp++ = space[i].color.blue;
            *cp   = space[i].color.alpha;
         }
         auto space_ = CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB);
         gradient =
            CGGradientCreateWithColorComponents(
               space_, components, locations, nspaces
            );
         CGColorSpaceRelease(space_);
      }
   }

   canvas::canvas_state::canvas_state()
   {
      _stack.push(std::make_unique<state_info>());
   }

   canvas::canvas_state::style const& canvas::canvas_state::fill_style() const
   {
      return current()->_fill_style;
   }

   void canvas::canvas_state::fill_style(style const& style)
   {
      current()->_fill_style = style;
   }

   void canvas::canvas_state::fill_style(style const& style, color_space const& space)
   {
      current()->_fill_style = style;
      make_gradient(space, current()->_fill_gradient);
   }

   CGGradientRef canvas::canvas_state::fill_gradient() const
   {
      return current()->_fill_gradient;
   }

   canvas::canvas_state::style const& canvas::canvas_state::stroke_style() const
   {
      return current()->_stroke_style;
   }

   void canvas::canvas_state::stroke_style(style const& style)
   {
      current()->_stroke_style = style;
   }

   void canvas::canvas_state::stroke_style(style const& style, color_space const& space)
   {
      current()->_stroke_style = style;
      make_gradient(space, current()->_stroke_gradient);
   }

   CGGradientRef canvas::canvas_state::stroke_gradient() const
   {
      return current()->_stroke_gradient;
   }

   class font const& canvas::canvas_state::font() const
   {
      return current()->_font;
   }

   void canvas::canvas_state::font(class font const& font_)
   {
      current()->_font = font_;
   }

   int canvas::canvas_state::text_align() const
   {
      return current()->_text_align;
   }

   void canvas::canvas_state::text_align(int align)
   {
      current()->_text_align = align;
   }

   canvas::canvas_state::mode_enum canvas::canvas_state::mode() const
   {
      return current()->_mode;
   }

   void canvas::canvas_state::mode(mode_enum mode_)
   {
      current()->_mode = mode_;
   }

   void canvas::canvas_state::save()
   {
      _stack.push(std::make_unique<state_info>(*current()));
   }

   void canvas::canvas_state::restore()
   {
      if (_stack.size())
         _stack.pop();
   }

   canvas::canvas(canvas_impl_ptr context_)
    : _context{ context_ }
    , _state{ std::make_unique<canvas_state>() }
   {
      // Flip text drawing vertically
      auto ctx = CGContextRef(_context);
      CGAffineTransform trans = CGAffineTransformMakeScale(1, -1);
      CGContextSetTextMatrix(ctx, trans);
   }

   canvas::~canvas()
   {
   }

   void canvas::translate(point p)
   {
      CGContextTranslateCTM(CGContextRef(_context), p.x, p.y);
   }

   void canvas::rotate(float rad)
   {
      CGContextRotateCTM(CGContextRef(_context), rad);
   }

   void canvas::scale(point p)
   {
      CGContextScaleCTM(CGContextRef(_context), p.x, p.y);
   }

   void canvas::save()
   {
      CGContextSaveGState(CGContextRef(_context));
      _state->save();
   }

   void canvas::restore()
   {
      CGContextRestoreGState(CGContextRef(_context));
      _state->restore();
   }

   void canvas::begin_path()
   {
      CGContextBeginPath(CGContextRef(_context));
   }

   void canvas::close_path()
   {
      CGContextClosePath(CGContextRef(_context));
   }

   namespace
   {
      bool needs_workaround(canvas::composite_op_enum mode)
      {
         switch (mode)
         {
            case canvas::composite_op_enum::source_in:
            case canvas::composite_op_enum::source_out:
            case canvas::composite_op_enum::destination_atop:
            case canvas::composite_op_enum::destination_in:
            case canvas::composite_op_enum::copy:
               return true;
            default:
               return false;
         };
      };
   }

   void canvas::fill_rule(path::fill_rule_enum rule)
   {
      _state->fill_rule(rule);
   }

   void canvas::fill()
   {
      auto apply_fill = [this](auto const& style)
      {
         using T = std::decay_t<decltype(style)>;
         if constexpr (std::is_same_v<T, color>)
         {
            auto ctx = CGContextRef(_context);
            CGContextSetRGBFillColor(ctx, style.red, style.green, style.blue, style.alpha);
            if (_state->fill_rule() == path::fill_winding)
               CGContextFillPath(ctx);
            else
               CGContextEOFillPath(ctx);
         }
         else if constexpr (std::is_same_v<T, linear_gradient>)
         {
            auto ctx = CGContextRef(_context);
            CGContextSaveGState(ctx);
            clip();  // Set to clip current path
            CGContextDrawLinearGradient(
               ctx, _state->fill_gradient(),
               CGPoint{ style.start.x, style.start.y },
               CGPoint{ style.end.x, style.end.y },
               kCGGradientDrawsAfterEndLocation | kCGGradientDrawsBeforeStartLocation
            );
            CGContextRestoreGState(ctx);
         }
         else if constexpr (std::is_same_v<T, radial_gradient>)
         {
            auto ctx = CGContextRef(_context);
            CGContextSaveGState(ctx);
            clip();  // Set to clip current path
            CGContextDrawRadialGradient(
               ctx, _state->fill_gradient(),
               CGPoint{ style.c1.x, style.c1.y }, style.c1_radius,
               CGPoint{ style.c2.x, style.c2.y }, style.c2_radius,
               kCGGradientDrawsAfterEndLocation | kCGGradientDrawsBeforeStartLocation);
            CGContextRestoreGState(ctx);
         }
      };

      std::visit(apply_fill, _state->fill_style());
   }

   void canvas::fill_preserve()
   {
      auto save = CGContextCopyPath(CGContextRef(_context));
      fill();
      CGContextAddPath(CGContextRef(_context), save);
      CGPathRelease(save);
   }

   void canvas::stroke()
   {
      auto apply_stroke = [this](auto const& style)
      {
         using T = std::decay_t<decltype(style)>;
         if constexpr (std::is_same_v<T, color>)
         {
            auto ctx = CGContextRef(_context);
            CGContextSetRGBStrokeColor(ctx, style.red, style.green, style.blue, style.alpha);
            CGContextStrokePath(ctx);
         }
         else if constexpr (std::is_same_v<T, linear_gradient>)
         {
            auto ctx = CGContextRef(_context);
            CGContextSaveGState(ctx);
            CGContextReplacePathWithStrokedPath(ctx);

            clip();  // Set to clip current path
            CGContextDrawLinearGradient(
               ctx, _state->stroke_gradient(),
               CGPoint{ style.start.x, style.start.y },
               CGPoint{ style.end.x, style.end.y },
               kCGGradientDrawsAfterEndLocation | kCGGradientDrawsBeforeStartLocation
            );
            CGContextRestoreGState(ctx);
         }
         else if constexpr (std::is_same_v<T, radial_gradient>)
         {
            auto ctx = CGContextRef(_context);
            CGContextSaveGState(ctx);
            CGContextReplacePathWithStrokedPath(ctx);

            clip();  // Set to clip current path
            CGContextDrawRadialGradient(
               ctx, _state->stroke_gradient(),
               CGPoint{ style.c1.x, style.c1.y }, style.c1_radius,
               CGPoint{ style.c2.x, style.c2.y }, style.c2_radius,
               kCGGradientDrawsAfterEndLocation | kCGGradientDrawsBeforeStartLocation
            );
            CGContextRestoreGState(ctx);
         }
      };

      std::visit(apply_stroke, _state->stroke_style());
   }

   void canvas::stroke_preserve()
   {
      auto save = CGContextCopyPath(CGContextRef(_context));
      stroke();
      CGContextAddPath(CGContextRef(_context), save);
      CGPathRelease(save);
   }

   void canvas::clip()
   {
      if (_state->fill_rule() == path::fill_winding)
         CGContextClip(CGContextRef(_context));
      else
         CGContextEOClip(CGContextRef(_context));
   }

   void canvas::move_to(point p)
   {
      CGContextMoveToPoint(CGContextRef(_context), p.x, p.y);
   }

   void canvas::line_to(point p)
   {
      CGContextAddLineToPoint(CGContextRef(_context), p.x, p.y);
   }

   void canvas::arc_to(point p1, point p2, float radius)
   {
      CGContextAddArcToPoint(
         CGContextRef(_context),
         p1.x, p1.y, p2.x, p2.y, radius
      );
   }

   void canvas::arc(
      point p, float radius,
      float start_angle, float end_angle,
      bool ccw
   )
   {
      // Note: For the clockwise, specify 0 to create a clockwise arc or 1
      // to create a counterclockwise arc. This is actually reversed from the
      // actual values because: "In a flipped coordinate system (the default
      // for UIView drawing methods in iOS), specifying a clockwise arc results
      // in a counterclockwise arc after the transformation is applied."
      CGContextAddArc(
         CGContextRef(_context),
         p.x, p.y, radius, start_angle, end_angle, ccw? 1 : 0
      );
   }

   namespace detail
   {
      void round_rect(canvas& c, rect bounds, float radius)
      {
         auto x = bounds.left;
         auto y = bounds.top;
         auto r = bounds.right;
         auto b = bounds.bottom;

         radius = std::min(radius, std::min(bounds.width(), bounds.height()));
         c.begin_path();
         c.move_to(point{ x, y + radius });
         c.line_to(point{ x, b - radius });
         c.arc_to(point{ x, b }, point{ x + radius, b }, radius);
         c.line_to(point{ r - radius, b });
         c.arc_to(point{ r, b }, point{ r, b - radius }, radius);
         c.line_to(point{ r, y + radius });
         c.arc_to(point{ r, y }, point{ r - radius, y }, radius);
         c.line_to(point{ x + radius, y });
         c.arc_to(point{ x, y }, point{ x, y + radius }, radius);
      }
   }

   void canvas::rect(struct rect r)
   {
      CGContextAddRect(CGContextRef(_context), CGRectMake(r.left, r.top, r.width(), r.height()));
   }

   void canvas::round_rect(struct rect r, float radius)
   {
      if (radius > 0.0f)
         detail::round_rect(*this, r, radius);
      else
         rect(r);
   }

   void canvas::path(class path const& p)
   {
      CGContextAddPath(CGContextRef(_context), p.impl());
   }

   void canvas::quadratic_curve_to(point cp, point end)
   {
      CGContextAddQuadCurveToPoint(CGContextRef(_context), cp.x, cp.y, end.x, end.y);
   }

   void canvas::bezier_curve_to(point cp1, point cp2, point end)
   {
      CGContextAddCurveToPoint(CGContextRef(_context), cp1.x, cp1.y, cp2.x, cp2.y, end.x, end.y);
   }

   void canvas::fill_style(color c)
   {
      _state->fill_style(c);
   }

   void canvas::stroke_style(color c)
   {
      _state->stroke_style(c);
   }

   void canvas::line_width(float w)
   {
      CGContextSetLineWidth(CGContextRef(_context), w);
   }

   void canvas::line_cap(line_cap_enum cap_)
   {
      CGLineCap cap;
      switch (cap_)
      {
         case butt:     cap = kCGLineCapButt; break;
         case round:    cap = kCGLineCapRound; break;
         case square:   cap = kCGLineCapSquare; break;
      }
      CGContextSetLineCap(CGContextRef(_context), cap);
   }

   void canvas::line_join(join_enum join_)
   {
      CGLineJoin join;
      switch (join_)
      {
         case bevel_join:  join = kCGLineJoinBevel; break;
         case round_join:  join = kCGLineJoinRound; break;
         case miter_join:  join = kCGLineJoinMiter; break;
      }
      CGContextSetLineJoin(CGContextRef(_context), join);
   }

   void canvas::miter_limit(float limit)
   {
      CGContextSetMiterLimit(CGContextRef(_context), limit);
   }

   void canvas::shadow_style(point offset, float blur, color c)
   {
      CGContextSetShadowWithColor(
         CGContextRef(_context), CGSizeMake(offset.x, -offset.y), blur,
         [
            [NSColor
               colorWithRed : c.red
                      green : c.green
                      blue  : c.blue
                      alpha : c.alpha
            ]
            CGColor
         ]
      );
   }

   void canvas::global_composite_operation(composite_op_enum mode)
   {
      _state->mode(mode);
      CGBlendMode cg_mode;
      switch (mode)
      {
         case source_over:          cg_mode = kCGBlendModeNormal; break;
         case source_atop:          cg_mode = kCGBlendModeSourceAtop; break;
         case source_in:            cg_mode = kCGBlendModeSourceIn; break;
         case source_out:           cg_mode = kCGBlendModeSourceOut; break;

         case destination_over:     cg_mode = kCGBlendModeDestinationOver; break;
         case destination_atop:     cg_mode = kCGBlendModeDestinationAtop; break;
         case destination_in:       cg_mode = kCGBlendModeDestinationIn; break;
         case destination_out:      cg_mode = kCGBlendModeDestinationOut; break;

         case lighter:              cg_mode = kCGBlendModePlusLighter; break;
         case darker:               cg_mode = kCGBlendModePlusDarker; break;
         case copy:                 cg_mode = kCGBlendModeCopy; break;
         case xor_:                 cg_mode = kCGBlendModeXOR; break;

         case difference:           cg_mode = kCGBlendModeDifference; break;
         case exclusion:            cg_mode = kCGBlendModeExclusion; break;
         case multiply:             cg_mode = kCGBlendModeMultiply; break;
         case screen:               cg_mode = kCGBlendModeScreen; break;

         case color_dodge:          cg_mode = kCGBlendModeColorDodge; break;
         case color_burn:           cg_mode = kCGBlendModeColorBurn; break;
         case soft_light:           cg_mode = kCGBlendModeSoftLight; break;
         case hard_light:           cg_mode = kCGBlendModeHardLight; break;

         case canvas::hue:          cg_mode = kCGBlendModeHue; break;
         case canvas::saturation:   cg_mode = kCGBlendModeSaturation; break;
         case canvas::color_op:     cg_mode = kCGBlendModeColor; break;
         case canvas::luminosity:   cg_mode = kCGBlendModeLuminosity; break;

      };
      CGContextSetBlendMode(CGContextRef(_context), cg_mode);
   }

   void canvas::fill_style(linear_gradient const& gr)
   {
      _state->fill_style(gr, gr.color_space);
   }

   void canvas::fill_style(radial_gradient const& gr)
   {
      _state->fill_style(gr, gr.color_space);
   }

   void canvas::stroke_style(linear_gradient const& gr)
   {
      _state->stroke_style(gr, gr.color_space);
   }

   void canvas::stroke_style(radial_gradient const& gr)
   {
      _state->stroke_style(gr, gr.color_space);
   }

   void canvas::font(class font const& font_)
   {
      if (font_)
         _state->font(font_);
   }

   namespace detail
   {
      CTLineRef measure_text(
         font const& font_
       , char const* f, char const* l
       , CGFloat& width, CGFloat& ascent, CGFloat& descent, CGFloat& leading
      )
      {
         NSFont* font = (__bridge NSFont*) font_.impl();
         CFStringRef keys[] = { kCTFontAttributeName, kCTForegroundColorFromContextAttributeName };
         CFTypeRef   values[] = { (__bridge const void*)font, kCFBooleanTrue };

         CFDictionaryRef font_attributes = CFDictionaryCreate(
            kCFAllocatorDefault, (const void**)&keys,
            (const void**)&values, sizeof(keys) / sizeof(keys[0]),
            &kCFTypeDictionaryKeyCallBacks,
            &kCFTypeDictionaryValueCallBacks
         );

         auto text = cf_string(f, l);
         auto attr_string =
            CFAttributedStringCreate(kCFAllocatorDefault, text, font_attributes);
         CFRelease(text);

         auto line = CTLineCreateWithAttributedString(attr_string);
         width = CTLineGetTypographicBounds(line, &ascent, &descent, &leading);
         CFRelease(attr_string);
         return line;
      }

      CTLineRef prepare_text(
         font const& font_
       , int text_align
       , point& p, char const* f, char const* l
      )
      {
         CGFloat ascent, descent, leading, width;
         auto line = measure_text(font_, f, l, width, ascent, descent, leading);
         switch (text_align & 0x1C)
         {
            case canvas::top:    p.y += ascent; break;
            case canvas::middle: p.y += (ascent - descent)/2; break;
            case canvas::bottom: p.y -= descent; break;
            default: break;
         }

         switch (text_align & 0x3)
         {
            case canvas::center: p.x -= width/2; break;
            case canvas::right:  p.x -= width; break;
            default: break;
         }

         return line;
      }

      void add_line_to_path(CGContextRef ctx, CTLineRef line)
      {
         auto run_array = CTLineGetGlyphRuns(line);
         auto path = CGPathCreateMutable();

         // for each RUN
         for (CFIndex run_index = 0; run_index < CFArrayGetCount(run_array); ++run_index)
         {
            // Get FONT for this run
            auto run = (CTRunRef) CFArrayGetValueAtIndex(run_array, run_index);
            auto runFont = (CTFontRef) CFDictionaryGetValue(CTRunGetAttributes(run), kCTFontAttributeName);
            auto glyph_count = CTRunGetGlyphCount(run);
            if (!glyph_count)
               continue;

            CGPoint pos;
            CTRunGetPositions(run, CFRangeMake(0, 1), &pos);
            CGAffineTransform t = CGAffineTransformMakeScale(1.0, -1.0);

            // for each GLYPH in run
            for (auto glyph_index = 0; glyph_index < glyph_count; glyph_index++)
            {
               // get Glyph & Glyph-data
               auto glyph_range = CFRangeMake(glyph_index, 1);
               CGGlyph glyph;
               CTRunGetGlyphs(run, glyph_range, &glyph);

               CGPoint glyph_pos;
               CTRunGetPositions(run, glyph_range, &glyph_pos);

               // Get PATH of outline
               {
                  auto glyph_path = CTFontCreatePathForGlyph(runFont, glyph, nullptr);
                  t = CGAffineTransformTranslate(t, glyph_pos.x-pos.x, glyph_pos.y-pos.y);
                  CGPathAddPath(path, &t, glyph_path);
                  CGPathRelease(glyph_path);
               }

               pos.x = glyph_pos.x;
               pos.y = glyph_pos.y;
            }
         }
         CGContextAddPath(ctx, path);
         CGPathRelease(path);
      }
   }

   void canvas::fill_text(std::string_view utf8, point p)
   {
      auto ctx = CGContextRef(_context);
      auto line = detail::prepare_text(
         _state->font(), _state->text_align()
       , p, utf8.begin(), utf8.end()
      );
      CGContextSetTextPosition(ctx, p.x, p.y);

      auto apply_gradient = [&](auto&& apply)
      {
         CGContextSaveGState(ctx);
         begin_path();
         translate({ p.x, p.y });                  // Move to p
         detail::add_line_to_path(ctx, line);      // Convert text to path and add
         clip();                                   // Set to clip current path
         apply();                                  // Apply the gradient
         CGContextRestoreGState(ctx);
      };

      auto apply_fill = [&](auto const& style)
      {
         using T = std::decay_t<decltype(style)>;
         if constexpr (std::is_same_v<T, color>)
         {
            CGContextSetRGBFillColor(ctx, style.red, style.green, style.blue, style.alpha);
            CGContextSetTextDrawingMode(ctx, kCGTextFill);
            CTLineDraw(line, ctx);
         }
         else if constexpr (std::is_same_v<T, linear_gradient>)
         {
            apply_gradient(
               [&] {
                  CGContextDrawLinearGradient(
                     ctx, _state->fill_gradient(),
                     CGPoint{ -p.x+style.start.x, -p.y+style.start.y },
                     CGPoint{ -p.x+style.end.x, -p.y+style.end.y },
                     kCGGradientDrawsAfterEndLocation | kCGGradientDrawsBeforeStartLocation
                  );
               }
            );
         }
         else if constexpr (std::is_same_v<T, radial_gradient>)
         {
            apply_gradient(
               [&] {
                  CGContextDrawRadialGradient(
                     ctx, _state->fill_gradient(),
                     CGPoint{ -p.x+style.c1.x, -p.y+style.c1.y }, style.c1_radius,
                     CGPoint{ -p.x+style.c2.x, -p.y+style.c2.y }, style.c2_radius,
                     kCGGradientDrawsAfterEndLocation | kCGGradientDrawsBeforeStartLocation
                  );
               }
            );
         }
      };

      std::visit(apply_fill, _state->fill_style());
      CFRelease(line);
   }

   void canvas::stroke_text(std::string_view utf8, point p)
   {
      auto ctx = CGContextRef(_context);
      auto line = detail::prepare_text(
         _state->font(), _state->text_align()
       , p, utf8.begin(), utf8.end()
      );
      CGContextSetTextPosition(ctx, p.x, p.y);

      auto apply_gradient = [&](auto&& apply)
      {
         CGContextSaveGState(ctx);
         begin_path();
         translate({ p.x, p.y });                  // Move to p
         detail::add_line_to_path(ctx, line);      // Convert text to path and add
         CGContextReplacePathWithStrokedPath(ctx); // Convert stroke to path
         clip();                                   // Set to clip current path
         apply();                                  // Apply the gradient
         CGContextRestoreGState(ctx);
      };

      auto apply_stroke = [&](auto const& style)
      {
         using T = std::decay_t<decltype(style)>;
         if constexpr (std::is_same_v<T, color>)
         {
            CGContextSetRGBStrokeColor(ctx, style.red, style.green, style.blue, style.alpha);
            CGContextSetTextDrawingMode(ctx, kCGTextStroke);
            CTLineDraw(line, ctx);
         }
         else if constexpr (std::is_same_v<T, linear_gradient>)
         {
            apply_gradient(
               [&] {
                  CGContextDrawLinearGradient(
                     ctx, _state->stroke_gradient(),
                     CGPoint{ -p.x+style.start.x, -p.y+style.start.y },
                     CGPoint{ -p.x+style.end.x, -p.y+style.end.y },
                     kCGGradientDrawsAfterEndLocation | kCGGradientDrawsBeforeStartLocation
                  );
               }
            );
         }
         else if constexpr (std::is_same_v<T, radial_gradient>)
         {
            apply_gradient(
               [&] {
                  CGContextDrawRadialGradient(
                     ctx, _state->stroke_gradient(),
                     CGPoint{ -p.x+style.c1.x, -p.y+style.c1.y }, style.c1_radius,
                     CGPoint{ -p.x+style.c2.x, -p.y+style.c2.y }, style.c2_radius,
                     kCGGradientDrawsAfterEndLocation | kCGGradientDrawsBeforeStartLocation
                  );
               }
            );
         }
      };

      std::visit(apply_stroke, _state->stroke_style());
      CFRelease(line);
   }

   canvas::text_metrics canvas::measure_text(std::string_view utf8)
   {
      auto ctx = CGContextRef(_context);
      CGFloat ascent, descent, leading, width;

      auto line = detail::measure_text(
         _state->font(), utf8.begin(), utf8.end(), width, ascent, descent, leading);

      auto bounds = CTLineGetImageBounds(line, ctx);
      CFRelease(line);
      return canvas::text_metrics
      {
         float(ascent)
       , float(descent)
       , float(leading)
       , { float(width), float(bounds.size.height) }
      };
   }

   void canvas::text_align(int align)
   {
      _state->text_align(align);
   }

   void canvas::text_align(text_halign align)
   {
      _state->text_align(_state->text_align() | align);
   }

   void canvas::text_baseline(text_valign align)
   {
      _state->text_align(_state->text_align() | align);
   }

   void canvas::draw(image const& img_, struct rect src, struct rect dest)
   {
      auto  img = (__bridge NSImage*) img_.impl();
      auto  src_ = NSRect{ { src.left, [img size].height - src.bottom }, { src.width(), src.height() } };
      auto  dest_ = NSRect{ { dest.left, dest.top }, { dest.width(), dest.height() } };

      NSCompositingOperation ns_mode;
      switch (_state->mode())
      {
         case source_over:          ns_mode = NSCompositingOperationSourceOver; break;
         case source_atop:          ns_mode = NSCompositingOperationSourceAtop; break;
         case source_in:            ns_mode = NSCompositingOperationSourceIn; break;
         case source_out:           ns_mode = NSCompositingOperationSourceOut; break;

         case destination_over:     ns_mode = NSCompositingOperationDestinationOver; break;
         case destination_atop:     ns_mode = NSCompositingOperationDestinationAtop; break;
         case destination_in:       ns_mode = NSCompositingOperationDestinationIn; break;
         case destination_out:      ns_mode = NSCompositingOperationDestinationOut; break;

         case lighter:              ns_mode = NSCompositingOperationPlusLighter; break;
         case darker:               ns_mode = NSCompositingOperationPlusDarker; break;
         case copy:                 ns_mode = NSCompositingOperationCopy; break;
         case xor_:                 ns_mode = NSCompositingOperationXOR; break;

         case difference:           ns_mode = NSCompositingOperationDifference; break;
         case exclusion:            ns_mode = NSCompositingOperationExclusion; break;
         case multiply:             ns_mode = NSCompositingOperationMultiply; break;
         case screen:               ns_mode = NSCompositingOperationScreen; break;

         case color_dodge:          ns_mode = NSCompositingOperationColorDodge; break;
         case color_burn:           ns_mode = NSCompositingOperationColorBurn; break;
         case soft_light:           ns_mode = NSCompositingOperationSoftLight; break;
         case hard_light:           ns_mode = NSCompositingOperationHardLight; break;

         case canvas::hue:          ns_mode = NSCompositingOperationHue; break;
         case canvas::saturation:   ns_mode = NSCompositingOperationSaturation; break;
         case canvas::color_op:     ns_mode = NSCompositingOperationColor; break;
         case canvas::luminosity:   ns_mode = NSCompositingOperationLuminosity; break;
      };

      [img drawInRect   :  dest_
         fromRect       :  src_
         operation      :  ns_mode
         fraction       :  1.0
         respectFlipped :  YES
         hints          :  nil
      ];
   }
}
