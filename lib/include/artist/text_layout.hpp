/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#if !defined(ELEMENTS_TEXT_LAYOUT_MARCH_17_2020)
#define ELEMENTS_TEXT_LAYOUT_MARCH_17_2020

#include <string_view>
#include <artist/font.hpp>
#include <artist/rect.hpp>
#include <memory>

namespace cycfi::artist
{
   class canvas;

   class text_layout
   {
   public:

      struct glyph_position
      {
         float start;
         float stop;
      };
                           text_layout(font const& font_, std::string_view utf8);
                           text_layout(text_layout const& rhs) = delete;
                           text_layout(text_layout&& rhs) noexcept = default;
                           ~text_layout();

      text_layout&         operator=(text_layout const& rhs) = delete;
      text_layout&         operator=(text_layout&& rhs) noexcept = default;

      void                 flow(float width, float indent = 0, float line_height = 0, bool justify = false);
      void                 draw(canvas& cnv, point p) const;
      glyph_position       position(char const* text) const;
      char const*          hit_test(point p) const;

   private:

      struct impl;
      using impl_ptr = std::unique_ptr<impl>;

      impl_ptr             _impl;
   };
}

#endif
