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

      using style_stack = std::stack<std::pair<style, style>>;

      style          _fill_style;
      CGGradientRef  _fill_gradient;
      style          _stroke_style;
      CGGradientRef  _stroke_gradient;
      style_stack    _style_stack;
   };

   canvas::canvas(host_context_ptr context_)
    : _context{ context_ }
    , _state{ std::make_unique<canvas_state>() }
   {
      // Flip the text drawing vertically
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
      _state->_style_stack.push(
         { _state->_fill_style, _state->_stroke_style }
      );
   }

   void canvas::restore()
   {
      CGContextRestoreGState(CGContextRef(_context));
      auto& [ fs, ss ] = _state->_style_stack.top();
      _state->_fill_style = std::move(fs);
      _state->_stroke_style = std::move(ss);
      _state->_style_stack.pop();
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
         if constexpr (std::is_same<T, color>::value)
         {
            auto ctx = CGContextRef(_context);
            CGContextSetRGBFillColor(ctx, style.red, style.green, style.blue, style.alpha);
            CGContextFillPath(ctx);
         }
         else if constexpr (std::is_same<T, linear_gradient>::value)
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
         else if constexpr (std::is_same<T, radial_gradient>::value)
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

      // if (_view._state->gradient && !(_view._state->paint ==  view_state::default_))
      // {
      //    auto  state = new_state();
      //    clip();  // Set to clip current path
      //    if (_view._state->paint == view_state::linear)
      //    {
      //       auto& gr = _view._state->linear_gradient;
      //       CGContextDrawLinearGradient(
      //          CGContextRef(_context), _view._state->gradient,
      //          CGPoint{ gr.start.x, gr.start.y },
      //          CGPoint{ gr.end.x, gr.end.y },
      //          kCGGradientDrawsAfterEndLocation);
      //    }
      //    else
      //    {
      //       auto& gr = _view._state->radial_gradient;
      //       CGContextDrawRadialGradient(
      //          CGContextRef(_context), _view._state->gradient,
      //          CGPoint{ gr.start.x, gr.start.y }, gr.start_radius,
      //          CGPoint{ gr.end.x, gr.end.y }, gr.end_radius,
      //          kCGGradientDrawsAfterEndLocation);

      //    }
      // }
      // else
      // {
         CGContextFillPath(CGContextRef(_context));
      // }
   }

   void canvas::fill_preserve()
   {
      auto save = CGContextCopyPath(CGContextRef(_context));
      fill();
      CGContextAddPath(CGContextRef(_context), save);
   }

   void canvas::stroke()
   {
      auto apply = [this](auto const& style)
      {
         using T = std::decay_t<decltype(style)>;
         if constexpr (std::is_same<T, color>::value)
         {
            auto ctx = CGContextRef(_context);
            CGContextSetRGBFillColor(ctx, style.red, style.green, style.blue, style.alpha);
            CGContextStrokePath(ctx);
         }
         else if constexpr (std::is_same<T, radial_gradient>::value)
         {
         }
         else if constexpr (std::is_same<T, linear_gradient>::value)
         {
         }
      };

      std::visit(apply, _state->_stroke_style);
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
      //CGContextEOClip(CGContextRef(_context));
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

      CGContextSetRGBStrokeColor(CGContextRef(_context), c.red, c.green, c.blue, c.alpha);
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

//    void canvas::color_space(color_stop const space[], std::size_t nspaces)
//    {
//       CGFloat   locations[nspaces];
//       CGFloat   components[nspaces * 4];
//       for (size_t i = 0; i != nspaces; ++i)
//       {
//          locations[i] = space[i].offset;
//          auto* cp = &components[i*4];
//          *cp++ = space[i].color.red;
//          *cp++ = space[i].color.green;
//          *cp++ = space[i].color.blue;
//          *cp   = space[i].color.alpha;
//       }
//       auto  space_ = CGColorSpaceCreateWithName(kCGColorSpaceGenericRGB);
//       _view._state->gradient = CGGradientCreateWithColorComponents(
//                                  space_, components, locations, nspaces);
//       CGColorSpaceRelease(space_);
//    }

//    void canvas::font(char const* family, float size_, int style_)
//    {
//       auto  family_ = [NSString stringWithUTF8String:family];
//       int   style = 0;
//       if (style_ & bold)
//          style |= NSBoldFontMask;
//       if (style_ & italic)
//          style |= NSItalicFontMask;

//       auto font_manager = [NSFontManager sharedFontManager];
//       auto font =
//          [font_manager
//             fontWithFamily : family_
//                     traits : style
//                     weight : 5
//                       size : size_
//          ];

//       if (font)
//       {
//          CFStringRef keys[] = { kCTFontAttributeName, kCTForegroundColorFromContextAttributeName };
//          CFTypeRef   values[] = { (__bridge const void*)font, kCFBooleanTrue };

//          if (_view._state->font_attributes)
//             CFRelease(_view._state->font_attributes);

//          _view._state->font_attributes = CFDictionaryCreate(
//            kCFAllocatorDefault, (const void**)&keys,
//            (const void**)&values, sizeof(keys) / sizeof(keys[0]),
//            &kCFTypeDictionaryKeyCallBacks,
//            &kCFTypeDictionaryValueCallBacks
//          );
//       }
//    }

//    CFStringRef cf_string(char const* f, char const* l = nullptr)
//    {
//       char* bytes;
//       std::size_t len = l? (l-f) : strlen(f);
//       bytes = (char*) CFAllocatorAllocate(CFAllocatorGetDefault(), len, 0);
//       strncpy(bytes, f, len);
//       return CFStringCreateWithCStringNoCopy(nullptr, bytes, kCFStringEncodingUTF8, nullptr);
//    }

//    namespace detail
//    {
//       CTLineRef measure_text(
//          CGContextRef ctx, view_state const* state
//        , char const* f, char const* l
//        , CGFloat width, CGFloat& ascent, CGFloat& descent, CGFloat& leading
//       )
//       {
//          auto text = cf_string(f, l);
//          auto attr_string =
//             CFAttributedStringCreate(kCFAllocatorDefault, text, state->font_attributes);
//          CFRelease(text);

//          auto line = CTLineCreateWithAttributedString(attr_string);
//          width = CTLineGetTypographicBounds(line, &ascent, &descent, &leading);
//          return line;
//       }

//       CTLineRef prepare_text(
//          CGContextRef ctx, view_state const* state
//        , point& p, char const* f, char const* l
//       )
//       {
//          CGFloat ascent, descent, leading, width;
//          auto line = measure_text(ctx, state, f, l, width, ascent, descent, leading);
//          switch (state->text_align & 0x1C)
//          {
//             case canvas::top:
//                p.y += ascent;
//                break;

//             case canvas::middle:
//                p.y += ascent/2 - descent/2;
//                break;

//             case canvas::bottom:
//                p.y -= descent;
//                break;

//             default:
//                break;
//          }

//          switch (state->text_align & 0x3)
//          {
//             case canvas::center:
//                p.x -= width/2;
//                break;

//             case canvas::right:
//                p.x -= width;
//                break;

//             default:
//                break;
//          }

//          return line;
//       }
//    }

//    void canvas::fill_text(point p, char const* f, char const* l)
//    {
//       auto ctx = CGContextRef(_context);
//       auto line = detail::prepare_text(ctx, _view._state.get(), p, f, l);
//       CGContextSetTextPosition(ctx, p.x, p.y);
//       CGContextSetTextDrawingMode(ctx, kCGTextFill);
//       CTLineDraw(line, ctx);
//       CFRelease(line);
//    }

//    void canvas::stroke_text(point p, char const* f, char const* l)
//    {
//       auto ctx = CGContextRef(_context);
//       auto line = detail::prepare_text(ctx, _view._state.get(), p, f, l);
//       CGContextSetTextPosition(ctx, p.x, p.y);
//       CGContextSetTextDrawingMode(ctx, kCGTextStroke);
//       CTLineDraw(line, ctx);
//       CFRelease(line);
//    }

//    canvas::text_metrics canvas::measure_text(char const* f, char const* l)
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

//    canvas::image::image(char const* filename)
//    {
//       auto filename_ = [NSString stringWithUTF8String:filename];
//       auto img_ = [[NSImage alloc] initWithContentsOfFile:filename_];
//       _rep = (__bridge_retained rep*) img_;
//    }

//    canvas::image::~image()
//    {
//       CFBridgingRelease(_rep);
//    }
//    photon::size canvas::image::size() const
//    {
//        auto size_ = [((__bridge NSImage*) _rep) size];
//        return { float(size_.width), float(size_.height) };
//    }

//    void canvas::draw(image const& img, photon::rect src, photon::rect dest)
//    {
//       // Sigh, Cocoa... everyone else is using top-left as x=0, y=0. You chose
//       // to do it the other way. Layout is so awkward using plain cartesian coordinates.
//       // It's not about purity. It's about practicality. Now we have to flip the
//       // coordinates for our images.

//       auto  img_ = (__bridge NSImage*) img._rep;
//       auto  src_ = NSRect{ src.left, [img_ size].height - src.bottom, src.width(), src.height() };
//       auto  dest_ = NSRect{ dest.left, dest.top, dest.width(), dest.height() };

//       [img_
//          drawInRect     :  dest_
//          fromRect       :  src_
//          operation      :  NSCompositeSourceAtop
//          fraction       :  1.0
//          respectFlipped :  YES
//          hints          :  nil
//       ];
//    }
}
