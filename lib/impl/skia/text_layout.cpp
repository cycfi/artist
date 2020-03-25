/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <string_view>
#include <artist/text_layout.hpp>
#include <artist/canvas.hpp>
#include <vector>

#include <SkShaper.h>

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
      }

      // void flow(float width, float indent, float line_height, bool justify)
      // {
      // }

      // void  draw(canvas& cnv, point p)
      // {
      // }

      // glyph_position position(char const* text) const
      // {
      //    return {};
      // }

      // char const* hit_test(point p) const
      // {
      //    return nullptr;
      // }

      font              _font;
      std::string_view  _utf8;
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
   }

   void text_layout::draw(canvas& cnv, point p) const
   {
   }

   text_layout::glyph_position text_layout::position(char const* text) const
   {
      return {};
   }

   char const* text_layout::hit_test(point p) const
   {
      return nullptr;
   }
}

