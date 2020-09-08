/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <string_view>
#include <artist/text_layout.hpp>
#include <artist/canvas.hpp>
#include <infra/utf8_utils.hpp>
#include "osx_utils.hpp"
#include <vector>
#include <algorithm>

namespace cycfi::artist
{
   class text_layout::impl
   {
   public:

      impl(font const& font_, std::u32string_view utf32);
      ~impl();

      struct row_info
      {
         point                pos;
         float                width;
         float                height;
         CTLineRef            line;
      };

      void                    flow(get_line_info const& glf, flow_info finfo);
      void                    draw(canvas& cnv, point p, color c);
      point                   caret_point(std::size_t index) const;
      std::size_t             caret_index(point p) const;
      std::size_t             num_lines() const;
      font&                   get_font();
      std::u32string const&   get_text() const;

   private:

      using rows = std::vector<row_info>;

      void                    clear_rows();

      class font              _font;
      std::u32string          _text;
      rows                    _rows;
   };

   text_layout::impl::impl(font const& font_, std::u32string_view utf32)
    : _font{ font_ }
    , _text{ utf32 }
   {
   }

   text_layout::impl::~impl()
   {
      clear_rows();
   }

   void text_layout::impl::clear_rows()
   {
      for (auto& line : _rows)
         CFRelease(line.line);
      _rows.clear();
   }

   void text_layout::impl::flow(get_line_info const& glf, flow_info finfo)
   {
      if (_text.size() == 0)
         return;
      clear_rows();

      NSFont* font = (__bridge NSFont*) _font.impl();
      CFStringRef keys[] = { kCTFontAttributeName, kCTForegroundColorFromContextAttributeName };
      CFTypeRef   values[] = { (__bridge const void*)font, kCFBooleanTrue };

      CFDictionaryRef font_attributes = CFDictionaryCreate(
         kCFAllocatorDefault, (const void**)&keys,
         (const void**)&values, sizeof(keys) / sizeof(keys[0]),
         &kCFTypeDictionaryKeyCallBacks,
         &kCFTypeDictionaryValueCallBacks
      );

      auto text = detail::cf_string(_text.data(), _text.data() + _text.size());
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

   void text_layout::impl::draw(canvas& cnv, point p, color c)
   {
      if (_rows.size() == 0)
         return;

      auto ctx = CGContextRef(cnv.impl());
      CGContextSetRGBFillColor(ctx, c.red, c.green, c.blue, c.alpha);

      for (auto const& line : _rows)
      {
         CGContextSetTextPosition(ctx, p.x+line.pos.x, p.y+line.pos.y);
         CTLineDraw(line.line, ctx);
      }
   }

   point text_layout::impl::caret_point(std::size_t index) const
   {
      if (_rows.size() == 0)
         return { 0, 0 };

      // Find the glyph index from string index
      auto char_index = index;
      auto row_index = -1;
      if (index >= _text.size())
      {
         char_index = _text.size();
         row_index = _rows.size() - 1;
      }

      // Find the row that includes the glyph index
      if (row_index == -1)
      {
         auto i = std::lower_bound(_rows.begin(), _rows.end(), char_index,
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
      auto offset = CTLineGetOffsetForStringIndex(row.line, char_index, nullptr);
      return { float(row.pos.x + offset), row.pos.y };
   }

   std::size_t text_layout::impl::caret_index(point p) const
   {
      if (_rows.size() == 0)
         return 0;

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
         return rng.location;

      if (i != _rows.end()-1 && p.x >= (i->pos.x + i->width))
         return rng.location + rng.length - 1;

      auto index = CTLineGetStringIndexForPosition(i->line, { p.x - i->pos.x, 0 });
      return index;
   }

   std::size_t text_layout::impl::num_lines() const
   {
      return _rows.size();
   }

   class font& text_layout::impl::get_font()
   {
      return _font;
   }

   std::u32string const& text_layout::impl::get_text() const
   {
      return _text;
   }

   ////////////////////////////////////////////////////////////////////////////
   text_layout::text_layout(font_descr font_, std::string_view utf8)
    : _impl{ std::make_unique<impl>(font_, to_utf32(utf8)) }
   {
   }

   text_layout::text_layout(font_descr font_, std::u32string_view utf32)
    : _impl{ std::make_unique<impl>(font_, utf32) }
   {
   }

   text_layout::~text_layout()
   {
   }

   text_layout::text_layout(text_layout&& rhs) noexcept
    : _impl{ std::move(rhs._impl) }
   {
   }

   void text_layout::text(std::string_view utf8)
   {
      _impl = std::make_unique<impl>(_impl->get_font(), to_utf32(utf8));
   }

   void text_layout::text(std::u32string_view utf32)
   {
      _impl = std::make_unique<impl>(_impl->get_font(), utf32);
   }

   std::u32string_view text_layout::text() const
   {
      return _impl->get_text();
   }

   void text_layout::flow(float width, bool justify)
   {
      auto line_info_f = [width](float y)
      {
         return line_info{ 0, width };
      };

      auto lh = _impl->get_font().line_height();
      _impl->flow(line_info_f, { justify, lh, lh });
   }

   void text_layout::flow(get_line_info const& glf, flow_info finfo)
   {
      _impl->flow(glf, finfo);
   }

   void text_layout::draw(canvas& cnv, point p, color c) const
   {
      _impl->draw(cnv, p, c);
   }

   std::size_t text_layout::num_lines() const
   {
      return _impl->num_lines();
   }

   point text_layout::caret_point(std::size_t index) const
   {
      return _impl->caret_point(index);
   }

   std::size_t text_layout::caret_index(point p) const
   {
      return _impl->caret_index(p);
   }
}

