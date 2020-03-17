/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <string_view>
#include <canvas/text_layout.hpp>
#include <canvas/canvas.hpp>
#include "osx_utils.hpp"
#include <vector>

namespace cycfi { namespace elements
{
   struct text_layout::impl
   {
      impl(font const& font_, std::string_view utf8)
       : _font{ font_ }
       , _utf8{ utf8 }
      {
      }

      ~impl()
      {
         for (auto& line : _rows)
            CFRelease(line);
      }

      void flow(float width, float indent, float line_height, bool justify)
      {
         _rows.clear();
         _width = width;
         _indent = indent;
         _line_height = line_height;

         NSFont* font = (__bridge NSFont*) _font.host_font();
         CFStringRef keys[] = { kCTFontAttributeName, kCTForegroundColorFromContextAttributeName };
         CFTypeRef   values[] = { (__bridge const void*)font, kCFBooleanTrue };

         CFDictionaryRef font_attributes = CFDictionaryCreate(
            kCFAllocatorDefault, (const void**)&keys,
            (const void**)&values, sizeof(keys) / sizeof(keys[0]),
            &kCFTypeDictionaryKeyCallBacks,
            &kCFTypeDictionaryValueCallBacks
         );

         auto text = detail::cf_string(_utf8.begin(), _utf8.end());
         auto attr_string =
            CFAttributedStringCreate(kCFAllocatorDefault, text, font_attributes);
         CFRelease(text);

         CFIndex start = 0;
         NSUInteger length = CFAttributedStringGetLength(attr_string);
         CTTypesetterRef typesetter = CTTypesetterCreateWithAttributedString(attr_string);
         while (start < length)
         {
            CFIndex count = CTTypesetterSuggestLineBreak(typesetter, start, width);
            CTLineRef line = CTTypesetterCreateLine(typesetter, CFRangeMake(start, count));

            if (justify)
            {
               // Full justify only if the line is not the end of the paragraph
               // and the line width is greater than 90% of the desired width.
               auto rng = CTLineGetStringRange(line);
               auto line_str = _utf8.substr(rng.location, rng.length);
               auto ch = line_str.back();
               if (ch != '\n' && ch != '\r')
               {
                  CGFloat ascent, descent, leading;
                  auto line_width = CTLineGetTypographicBounds(line, &ascent, &descent, &leading);
                  if ((line_width / width) > 0.9)
                  {
                     CTLineRef justified = CTLineCreateJustifiedLine(line, 1.0, width);
                     CFRelease(line);
                     line = justified;
                  }
               }
            }
            _rows.push_back(line);
            start += count;
         }
      }

      void  draw(canvas& cnv, point p)
      {
         float x = _indent;
         float y = 0;
         auto ctx = CGContextRef(cnv.host_context());
         for (auto const& line : _rows)
         {
            CGContextSetTextPosition(ctx, p.x+x, p.y+y);
            x = 0;
            y += _line_height;
            CTLineDraw(line, ctx);
         }
      }

      glyph_position position(char const* text) const
      {
         return {};
      }

      char const* hit_test(point p) const
      {
         return nullptr;
      }

      using rows = std::vector<CTLineRef>;

      font              _font;
      std::string_view  _utf8;
      rows              _rows;
      float             _width;
      float             _indent;
      float             _line_height;
   };

   text_layout::text_layout(font const& font_, std::string_view utf8)
    : _impl{ std::make_unique<impl>(font_, utf8) }
   {
   }

   text_layout::~text_layout()
   {
   }

   void text_layout::flow(float width, float indent, float line_height, bool justify)
   {
      if (line_height == 0)
      {
         auto m = _impl->_font.metrics();
         line_height = m.ascent + m.descent + m.leading;
      }
      _impl->flow(width, indent, line_height, justify);
   }

   void text_layout::draw(canvas& cnv, point p) const
   {
      _impl->draw(cnv, p);
   }

   text_layout::glyph_position text_layout::position(char const* text) const
   {
      return _impl->position(text);
   }

   char const* text_layout::hit_test(point p) const
   {
      return _impl->hit_test(p);
   }
}}

