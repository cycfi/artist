/*=============================================================================
   Copyright (c) 2016-2024 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
// TODO(cairo): text_layout is a stub. See Stage 5 for implementation.
// Full text layout requires HarfBuzz shaping and Cairo glyph rendering.
// See Skia text_layout.cpp for the expected behavior.
#include <artist/text_layout.hpp>
#include <artist/canvas.hpp>
#include <string>

namespace cycfi::artist
{
   class text_layout::impl
   {
      // TODO(cairo): Implement using HarfBuzz + Cairo glyph API in Stage 5.
   };

   text_layout::text_layout(font_descr /*font_*/, std::string_view /*utf8*/)
   {
   }

   text_layout::text_layout(font_descr /*font_*/, std::u32string_view /*utf32*/)
   {
   }

   text_layout::text_layout(text_layout&& rhs) noexcept
    : _impl(std::move(rhs._impl))
   {
   }

   text_layout::~text_layout()
   {
   }

   void text_layout::text(std::string_view /*utf8*/)
   {
   }

   void text_layout::text(std::u32string_view /*utf32*/)
   {
   }

   std::u32string_view text_layout::text() const
   {
      return {};
   }

   void text_layout::flow(float /*width*/, bool /*justify*/)
   {
   }

   void text_layout::flow(get_line_info const& /*glf*/, flow_info /*finfo*/)
   {
   }

   void text_layout::draw(canvas& /*cnv*/, point /*p*/, color /*c*/) const
   {
      // TODO(cairo): text_layout::draw is not implemented. No output is produced.
      // Callers expecting text layout rendering will see nothing.
      // See ai/artist-cairo-backend-assimilation-plan.md Stage 5.
   }

   std::size_t text_layout::num_lines() const
   {
      return 0;
   }

   point text_layout::caret_point(std::size_t /*index*/) const
   {
      return {};
   }

   std::size_t text_layout::caret_index(point /*p*/) const
   {
      return npos;
   }

   text_layout::break_enum text_layout::line_break(std::size_t /*index*/) const
   {
      return indeterminate;
   }

   text_layout::break_enum text_layout::word_break(std::size_t /*index*/) const
   {
      return indeterminate;
   }
}
