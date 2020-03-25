/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <string_view>
#include <artist/text_layout.hpp>
#include <artist/canvas.hpp>
#include "osx_utils.hpp"
#include <vector>

namespace cycfi::artist
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
            CFRelease(line.second);
      }

      void flow(get_line_info const& glf, flow_info finfo)
      {
         _rows.clear();

         NSFont* font = (__bridge NSFont*) _font.impl();
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
         float ypos = 0;
         auto l_info = glf(ypos);

         NSUInteger length = CFAttributedStringGetLength(attr_string);
         CTTypesetterRef typesetter = CTTypesetterCreateWithAttributedString(attr_string);
         while (start < length)
         {
            CFIndex count = CTTypesetterSuggestLineBreak(typesetter, start, l_info.width);
            CTLineRef line = CTTypesetterCreateLine(typesetter, CFRangeMake(start, count));

            if (finfo.justify)
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
                  if ((line_width / l_info.width) > 0.9)
                  {
                     CTLineRef justified = CTLineCreateJustifiedLine(line, 1.0, l_info.width);
                     CFRelease(line);
                     line = justified;
                  }
               }
            }
            start += count;
            _rows.push_back(std::make_pair(point{ l_info.offset, ypos }, line));
            ypos += finfo.line_height;
            l_info = glf(ypos);
         }
         _rows.back().first.y += finfo.last_line_height - finfo.line_height;
      }

      void  draw(canvas& cnv, point p)
      {
         auto ctx = CGContextRef(cnv.impl());
         for (auto const& line : _rows)
         {
            CGContextSetTextPosition(ctx, p.x+line.first.x, p.y+line.first.y);
            CTLineDraw(line.second, ctx);
         }
      }

      using rows = std::vector<std::pair<point, CTLineRef>>;

      font              _font;
      std::string_view  _utf8;
      rows              _rows;
   };

   text_layout::text_layout(font const& font_, std::string_view utf8)
    : _impl{ std::make_unique<impl>(font_, utf8) }
   {
   }

   text_layout::~text_layout()
   {
   }

   void text_layout::flow(get_line_info const& glf, flow_info finfo)
   {
      _impl->flow(glf, finfo);
   }

   void text_layout::flow(float width, bool justify)
   {
      auto line_info_f = [this, width](float y)
      {
         return line_info{ 0, width };
      };

      auto lh = _impl->_font.line_height();
      _impl->flow(line_info_f, { justify, lh, lh });
   }

   void text_layout::draw(canvas& cnv, point p) const
   {
      _impl->draw(cnv, p);
   }
}

