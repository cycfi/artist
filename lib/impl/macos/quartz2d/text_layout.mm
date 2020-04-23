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
   class text_layout::impl
   {
   public:

      impl(font const& font_, std::string_view utf8);
      impl(font const& font_, color c, std::string_view utf8);
      ~impl();

      struct row_info
      {
         point          pos;
         float          width;
         float          height;
         CTLineRef      line;
      };

      void              clear_rows();
      void              flow(get_line_info const& glf, flow_info finfo);
      void              draw(canvas& cnv, point p);
      void              text(std::string_view utf8);
      void              build_indices();
      point             caret_pos(std::size_t str_pos) const;
      std::size_t       hit_test(point p) const;
      std::size_t       num_lines() const;
      class font&       font();

   private:

      using rows = std::vector<row_info>;
      using indices = std::vector<std::size_t>;

      class font        _font;
      color             _text_color;
      std::string_view  _utf8;
      rows              _rows;
      indices           _indices;
   };

   text_layout::impl::impl(class font const& font_, color c, std::string_view utf8)
    : _font{ font_ }
    , _text_color{ c }
    , _utf8{ utf8 }
   {
      build_indices();
   }

   text_layout::impl::impl(class font const& font_, std::string_view utf8)
    : impl{ font_, colors::black, utf8 }
   {}

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
      clear_rows();
      if (_utf8.size() == 0)
         return;

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

   void text_layout::impl::draw(canvas& cnv, point p)
   {
      if (_rows.size() == 0)
         return;

      auto ctx = CGContextRef(cnv.impl());
      CGContextSetRGBFillColor(
         ctx, _text_color.red, _text_color.green, _text_color.blue, _text_color.alpha
      );

      for (auto const& line : _rows)
      {
         CGContextSetTextPosition(ctx, p.x+line.pos.x, p.y+line.pos.y);
         CTLineDraw(line.line, ctx);
      }
   }

   void text_layout::impl::text(std::string_view utf8)
   {
      clear_rows();
      _indices.clear();
      _utf8 = utf8;
      build_indices();
   }

   void text_layout::impl::build_indices()
   {
      _indices.clear();
      if (_utf8.size() == 0)
         return;

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

   point text_layout::impl::caret_pos(std::size_t str_pos) const
   {
      if (_rows.size() == 0)
         return { 0, 0 };

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
         auto i = std::lower_bound(_rows.begin(), _rows.end(), glyph_index,
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

   std::size_t text_layout::impl::hit_test(point p) const
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
         return _indices[rng.location];

      if (i != _rows.end()-1 && p.x >= (i->pos.x + i->width))
         return _indices[rng.location + rng.length - 1];

      auto index = CTLineGetStringIndexForPosition(i->line, { p.x - i->pos.x, 0 });
      return _indices[index];
   }

   std::size_t text_layout::impl::num_lines() const
   {
      return _rows.size();
   }

   class font& text_layout::impl::font()
   {
      return _font;
   }

   text_layout::text_layout(font const& font_, std::string_view utf8)
    : _impl{ std::make_unique<impl>(font_, utf8) }
   {
   }

   text_layout::text_layout(font const& font_, color c, std::string_view utf8)
    : _impl{ std::make_unique<impl>(font_, c, utf8) }
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
      _impl->text(utf8);
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

      auto lh = _impl->font().line_height();
      _impl->flow(line_info_f, { justify, lh, lh });
   }

   void text_layout::draw(canvas& cnv, point p) const
   {
      _impl->draw(cnv, p);
   }

   std::size_t text_layout::num_lines() const
   {
      return _impl->num_lines();
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

