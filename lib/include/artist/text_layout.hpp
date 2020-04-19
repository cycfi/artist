/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#if !defined(ARTIST_TEXT_LAYOUT_MARCH_17_2020)
#define ARTIST_TEXT_LAYOUT_MARCH_17_2020

#include <string_view>
#include <artist/font.hpp>
#include <artist/rect.hpp>
#include <memory>
#include <functional>

namespace cycfi::artist
{
   class canvas;
   class text_layout;

   class text_layout
   {
   public:
                           text_layout(font const& font_, std::string_view utf8);
                           text_layout(text_layout const& rhs) = delete;
                           text_layout(text_layout&& rhs) noexcept = default;
                           ~text_layout();

      text_layout&         operator=(text_layout const& rhs) = delete;
      text_layout&         operator=(text_layout&& rhs) noexcept = default;

      struct line_info
      {
         float    offset;
         float    width;
      };

      struct flow_info
      {
         bool     justify;
         float    line_height;
         float    last_line_height;
      };

      using get_line_info = std::function<line_info(float y)>;

      void                 flow(float width, bool justify = false);
      void                 flow(get_line_info const& glf, flow_info finfo);
      void                 draw(canvas& cnv, point p) const;
      void                 draw(canvas& cnv, float x, float y) const;

   private:

      struct impl;
      using impl_ptr = std::unique_ptr<impl>;

      impl_ptr             _impl;
   };

   ////////////////////////////////////////////////////////////////////////////
   // Inlines
   ////////////////////////////////////////////////////////////////////////////
   inline void text_layout::draw(canvas& cnv, float x, float y) const
   {
      draw(cnv, { x, y });
   }
}

#endif
