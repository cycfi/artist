/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <canvas/canvas.hpp>
#include <Quartz/Quartz.h>
#include <stack>
#include <variant>

namespace cycfi::elements
{
   struct canvas::canvas_state
   {
      using style = std::variant<
         color
       , canvas::linear_gradient
       , canvas::radial_gradient
      >;

      struct aux
      {
         style       _fill_style;
         style       _stroke_style;
         struct font _font;
         int         _text_align;
      };

      using aux_stack = std::stack<aux>;

      style          _fill_style = colors::black;
      CGGradientRef  _fill_gradient = nullptr;
      style          _stroke_style = colors::black;
      CGGradientRef  _stroke_gradient = nullptr;
      struct font    _font = font_descr{ "Helvetica Neue", 12 };
      int            _text_align = canvas::baseline;
      aux_stack      _aux_stack;
   };

   canvas::canvas(host_context_ptr context_)
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
      _state->_aux_stack.push(
         {
            _state->_fill_style,
            _state->_stroke_style,
            _state->_font
         }
      );
   }

   void canvas::restore()
   {
      CGContextRestoreGState(CGContextRef(_context));
      auto& [ fs, ss, f, ta ] = _state->_aux_stack.top();
      _state->_fill_style     = std::move(fs);
      _state->_stroke_style   = std::move(ss);
      _state->_font           = std::move(f);
      _state->_text_align     = ta;
      _state->_aux_stack.pop();
   }

   void canvas::begin_path()
   {
      CGContextBeginPath(CGContextRef(_context));
   }

   void canvas::close_path()
   {
      CGContextClosePath(CGContextRef(_context));
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
            CGContextFillPath(ctx);
         }
         else if constexpr (std::is_same_v<T, linear_gradient>)
         {
            auto  state = new_state();
            clip();  // Set to clip current path
            CGContextDrawLinearGradient(
               CGContextRef(_context), _state->_fill_gradient,
               CGPoint{ style.start.x, style.start.y },
               CGPoint{ style.end.x, style.end.y },
               kCGGradientDrawsAfterEndLocation | kCGGradientDrawsBeforeStartLocation
            );
         }
         else if constexpr (std::is_same_v<T, radial_gradient>)
         {
            auto  state = new_state();
            clip();  // Set to clip current path
            CGContextDrawRadialGradient(
               CGContextRef(_context), _state->_fill_gradient,
               CGPoint{ style.c1.x, style.c1.y }, style.c1_radius,
               CGPoint{ style.c2.x, style.c2.y }, style.c2_radius,
               kCGGradientDrawsAfterEndLocation | kCGGradientDrawsBeforeStartLocation);
         }
      };

      std::visit(apply_fill, _state->_fill_style);
   }

   void canvas::fill_preserve()
   {
      auto save = CGContextCopyPath(CGContextRef(_context));
      fill();
      CGContextAddPath(CGContextRef(_context), save);
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
            auto  state = new_state();
            auto ctx = CGContextRef(_context);
            CGContextReplacePathWithStrokedPath(ctx);

            clip();  // Set to clip current path
            CGContextDrawLinearGradient(
               ctx, _state->_stroke_gradient,
               CGPoint{ style.start.x, style.start.y },
               CGPoint{ style.end.x, style.end.y },
               kCGGradientDrawsAfterEndLocation | kCGGradientDrawsBeforeStartLocation
            );
         }
         else if constexpr (std::is_same_v<T, radial_gradient>)
         {
            auto  state = new_state();
            auto ctx = CGContextRef(_context);
            CGContextReplacePathWithStrokedPath(ctx);

            clip();  // Set to clip current path
            CGContextDrawRadialGradient(
               ctx, _state->_stroke_gradient,
               CGPoint{ style.c1.x, style.c1.y }, style.c1_radius,
               CGPoint{ style.c2.x, style.c2.y }, style.c2_radius,
               kCGGradientDrawsAfterEndLocation | kCGGradientDrawsBeforeStartLocation
            );
         }
      };

      std::visit(apply_stroke, _state->_stroke_style);
   }

   void canvas::stroke_preserve()
   {
      auto save = CGContextCopyPath(CGContextRef(_context));
      stroke();
      CGContextAddPath(CGContextRef(_context), save);
   }

   void canvas::clip()
   {
      CGContextClip(CGContextRef(_context));
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
      CGContextAddArc(
         CGContextRef(_context),
         p.x, p.y, radius, start_angle, end_angle, !ccw
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
      _state->_fill_style = c;
   }

   void canvas::stroke_style(color c)
   {
      _state->_stroke_style = c;
   }

   void canvas::line_width(float w)
   {
      CGContextSetLineWidth(CGContextRef(_context), w);
   }

//    void canvas::shadow_style(point p, float blur, color c)
//    {
//       CGContextSetShadowWithColor(
//          CGContextRef(_context), CGSizeMake(p.x, -p.y), blur,
//          [
//             [NSColor
//                colorWithRed : c.red
//                       green : c.green
//                       blue  : c.blue
//                       alpha : c.alpha
//             ]
//             CGColor
//          ]
//       );
//    }

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

   void canvas::fill_style(linear_gradient const& gr)
   {
      _state->_fill_style = gr;
      make_gradient(gr.space, _state->_fill_gradient);
   }

   void canvas::fill_style(radial_gradient const& gr)
   {
      _state->_fill_style = gr;
      make_gradient(gr.space, _state->_fill_gradient);
   }

   void canvas::stroke_style(linear_gradient const& gr)
   {
      _state->_stroke_style = gr;
      make_gradient(gr.space, _state->_stroke_gradient);
   }

   void canvas::stroke_style(radial_gradient const& gr)
   {
      _state->_stroke_style = gr;
      make_gradient(gr.space, _state->_stroke_gradient);
   }

   void canvas::font(struct font const& font_)
   {
      _state->_font = font_;
   }

   CFStringRef cf_string(char const* f, char const* l)
   {
      return CFStringCreateWithBytesNoCopy(
         nullptr, (UInt8 const*)f, l-f, kCFStringEncodingUTF8
       , false, kCFAllocatorNull
      );
   }

   namespace detail
   {
      CTLineRef measure_text(
         font const& font_
       , char const* f, char const* l
       , CGFloat width, CGFloat& ascent, CGFloat& descent, CGFloat& leading
      )
      {
         CFDictionaryRef font_attributes = (CFDictionaryRef) font_.host_font();
         auto text = cf_string(f, l);
         auto attr_string =
            CFAttributedStringCreate(kCFAllocatorDefault, text, font_attributes);
         CFRelease(text);

         auto line = CTLineCreateWithAttributedString(attr_string);
         width = CTLineGetTypographicBounds(line, &ascent, &descent, &leading);
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
            case canvas::top:
               p.y += ascent;
               break;

            case canvas::middle:
               p.y += ascent/2 - descent/2;
               break;

            case canvas::bottom:
               p.y -= descent;
               break;

            default:
               break;
         }

         switch (text_align & 0x3)
         {
            case canvas::center:
               p.x -= width/2;
               break;

            case canvas::right:
               p.x -= width;
               break;

            default:
               break;
         }

         return line;
      }

      CGMutablePathRef line_to_path(CTLineRef line)
      {
         CFArrayRef runArray = CTLineGetGlyphRuns(line);
         CGMutablePathRef path = CGPathCreateMutable();

         // for each RUN
         for (CFIndex runIndex = 0; runIndex < CFArrayGetCount(runArray); runIndex++)
         {
            // Get FONT for this run
            CTRunRef run = (CTRunRef) CFArrayGetValueAtIndex(runArray, runIndex);
            CTFontRef runFont = (CTFontRef) CFDictionaryGetValue(CTRunGetAttributes(run), kCTFontAttributeName);

            // for each GLYPH in run
            for (CFIndex runGlyphIndex = 0; runGlyphIndex < CTRunGetGlyphCount(run); runGlyphIndex++)
            {
               // get Glyph & Glyph-data
               CFRange thisGlyphRange = CFRangeMake(runGlyphIndex, 1);
               CGGlyph glyph;
               CGPoint position;
               CTRunGetGlyphs(run, thisGlyphRange, &glyph);
               CTRunGetPositions(run, thisGlyphRange, &position);

               // Get PATH of outline
               {
                  CGPathRef letter = CTFontCreatePathForGlyph(runFont, glyph, NULL);
                  CGAffineTransform t = CGAffineTransformMakeScale(1.0, -1.0);
                  t = CGAffineTransformTranslate(t, position.x, position.y);
                  // CGAffineTransformMakeTranslation(position.x + p.x, position.y + p.y);
                  CGPathAddPath(path, &t, letter);
                  CGPathRelease(letter);
               }
            }
         }
         return path;
      }
   }

   void canvas::fill_text(std::string_view utf8, point p)
   {
      auto ctx = CGContextRef(_context);
      auto line = detail::prepare_text(
         _state->_font, _state->_text_align
       , p, utf8.begin(), utf8.end()
      );
      CGContextSetTextPosition(ctx, p.x, p.y);

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
            auto  state = new_state();
            auto ctx = CGContextRef(_context);

            begin_path();
            auto path = detail::line_to_path(line);   // Convert text to path and add

            // scale({ 1.0, -1.0 });
            translate({ p.x, p.y });


            CGContextAddPath(ctx, path);              // Add path


            // translate({ -p.x, p.y });
            // scale({ -1.0, 1.0 });

            // rect({ p.x, p.y -50, p.x+150, p.y+20 });
            clip();                                   // Set to clip current path

            CGContextDrawLinearGradient(
               CGContextRef(_context), _state->_fill_gradient,
               CGPoint{ -p.x+style.start.x, -p.y+style.start.y },
               CGPoint{ -p.x+style.end.x, -p.y+style.end.y },
               kCGGradientDrawsAfterEndLocation | kCGGradientDrawsBeforeStartLocation
            );
         }
         else if constexpr (std::is_same_v<T, radial_gradient>)
         {
         }
      };

      std::visit(apply_fill, _state->_fill_style);
      CFRelease(line);
   }

   void canvas::stroke_text(std::string_view utf8, point p)
   {
      auto ctx = CGContextRef(_context);
      auto line = detail::prepare_text(
         _state->_font, _state->_text_align
       , p, utf8.begin(), utf8.end()
      );
      CGContextSetTextPosition(ctx, p.x, p.y);

      auto apply_stroke = [&](auto const& style)
      {
         using T = std::decay_t<decltype(style)>;
         if constexpr (std::is_same_v<T, color>)
         {
            CGContextSetRGBStrokeColor(ctx, style.red, style.green, style.blue, style.alpha);
            CGContextSetTextDrawingMode(ctx, kCGTextStroke);
            CTLineDraw(line, ctx);
            CFRelease(line);
         }
         else if constexpr (std::is_same_v<T, linear_gradient>)
         {
         }
         else if constexpr (std::is_same_v<T, radial_gradient>)
         {
         }
      };

      std::visit(apply_stroke, _state->_stroke_style);
   }

//    canvas::text_metrics canvas::measure_text(std::string_view utf8)
//    {
//       auto ctx = CGContextRef(_context);
//       CGFloat ascent, descent, leading, width;

//       auto line = detail::measure_text(
//          ctx, _view._state.get(), f, l, width, ascent, descent, leading);

//       auto bounds = CTLineGetImageBounds(line, ctx);
//       CFRelease(line);
//       return canvas::text_metrics
//       {
//          float(ascent), float(descent),
//          float(leading), float(width),
//          {
//             float(bounds.origin.x), float(bounds.origin.y),
//             float(bounds.size.width), float(bounds.size.height)
//          }
//       };
//    }

//    void canvas::text_align(int align)
//    {
//       _view._state->text_align = align;
//    }

   void canvas::draw(picture const& pic, struct rect src, struct rect dest)
   {
      auto  img = (__bridge NSImage*) pic.host_picture();
      auto  src_ = NSRect{ src.left, [img size].height - src.bottom, src.width(), src.height() };
      auto  dest_ = NSRect{ dest.left, dest.top, dest.width(), dest.height() };

      [img
         drawInRect     :  dest_
         fromRect       :  src_
         operation      :  NSCompositingOperationSourceOver
         fraction       :  1.0
         respectFlipped :  YES
         hints          :  nil
      ];

   }
}
