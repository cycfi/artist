/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <string_view>
#include <artist/text_layout.hpp>
#include <artist/canvas.hpp>
#include <artist/detail/utf8_utils.hpp>
#include "osx_utils.hpp"
#include <vector>
#include <algorithm>

namespace cycfi::artist
{
   struct text_layout::impl
   {
      impl(font const& font_, std::string_view utf8)
       : _font{ font_ }
       , _utf8{ utf8 }
      {
         build_indices();
      }

      ~impl()
      {
         for (auto& line : _rows)
            CFRelease(line.line);
      }

      struct row_info
      {
         point          pos;
         float          width;
         float          height;
         CTLineRef      line;
      };

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
            CGFloat ascent, descent, leading;
            auto line_width = CTLineGetTypographicBounds(line, &ascent, &descent, &leading);

            if (finfo.justify)
            {
               // Full justify only if the line is not the end of the paragraph
               // and the line width is greater than 90% of the desired width.
               // auto rng = CTLineGetStringRange(line);
               NSString* string = [(__bridge NSAttributedString*)attr_string string];
               auto end_pos = start + count;
               auto ch = (end_pos == length)? 0 : [string characterAtIndex : start + count];

               if (ch && ch != '\n' && ch != '\r')
               {
                  if ((line_width / l_info.width) > 0.9)
                  {
                     CTLineRef justified = CTLineCreateJustifiedLine(line, 1.0, l_info.width);
                     CFRelease(line);
                     line = justified;
                     line_width = l_info.width;
                  }
               }
            }
            _rows.emplace_back(
               row_info{
                  point{ l_info.offset, ypos }
                , float(line_width)
                , finfo.line_height
                , line
               }
            );
            start += count;
            ypos += finfo.line_height;
            l_info = glf(ypos);
         }
         auto& last = _rows.back();
         last.pos.y += finfo.last_line_height - finfo.line_height;
         last.height = finfo.last_line_height;
         CFRelease(attr_string);
      }

      void draw(canvas& cnv, point p)
      {
         auto ctx = CGContextRef(cnv.impl());
         for (auto const& line : _rows)
         {
            CGContextSetTextPosition(ctx, p.x+line.pos.x, p.y+line.pos.y);
            CTLineDraw(line.line, ctx);
         }
      }

      void build_indices()
      {
         if (_indices.empty())
         {
            // Build the utf8 indices vector
            char const* i = _utf8.data();
            char const* last = i + _utf8.size();
            while (i != last)
            {
               auto next = next_utf8(i, last);
               _indices.push_back(i-_utf8.data());
               i = next;
            }
            _indices.push_back(_utf8.size());
         }
      }

      point caret_pos(std::size_t str_pos) const
      {
         // Find the glyph index from str_pos
         auto glyph_index = str_pos;
         auto row_index = -1;
         if (str_pos < _utf8.size())
         {
            auto f = _indices.begin() + (str_pos / 4);
            auto l = _indices.begin() + str_pos + 1;
            auto i = std::lower_bound(f, l, str_pos,
               [](std::size_t index, std::size_t pos)
               {
                  return index < pos;
               }
            );
            if (i == _indices.end())
               return { -1, -1 };
            glyph_index = i - _indices.begin();
         }
         else
         {
            glyph_index = _indices.back();
            row_index = _rows.size() - 1;
         }

         // Find the row that includes the glyph index
         if (row_index == -1)
         {
            auto i = std::lower_bound(_rows.begin(), _rows.end(), str_pos,
               [](auto const& row, std::size_t pos)
               {
                  auto rng = CTLineGetStringRange(row.line);
                  return (rng.location + rng.length - 1) < pos;
               }
            );
            if (i == _rows.end())
               return { -1, -1 };
            row_index = i - _rows.begin();
         }
         // Now find the glyph position in the row
         auto const& row = _rows[row_index];
         auto offset = CTLineGetOffsetForStringIndex(row.line, glyph_index, nullptr);
         return { float(row.pos.x + offset), row.pos.y };
      }

      std::size_t hit_test(point p) const
      {
         auto i = std::lower_bound(
            _rows.begin(), _rows.end(), p.y,
            [](auto const& row, float y)
            {
               return row.pos.y < y;
            }
         );
         if (i == _rows.end())
            return npos;

         auto rng = CTLineGetStringRange(i->line);
         if (p.x <= i->pos.x)
            return _indices[rng.location];

         if (i != _rows.end()-1 && p.x >= (i->pos.x + i->width))
            return _indices[rng.location + rng.length - 1];

         auto index = CTLineGetStringIndexForPosition(i->line, { p.x - i->pos.x, 0 });
         return _indices[index];
      }

      using rows = std::vector<row_info>;
      using indices = std::vector<std::size_t>;

      font              _font;
      std::string_view  _utf8;
      rows              _rows;
      indices           _indices;
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

   point text_layout::caret_pos(std::size_t str_pos) const
   {
      return _impl->caret_pos(str_pos);
   }

   std::size_t text_layout::hit_test(point p) const
   {
      return _impl->hit_test(p);
   }
}

