/*=============================================================================
   Copyright (c) 2016-2024 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
// TODO(cairo): text_layout is a stub. See Stage 5 for implementation.
// Full text layout requires HarfBuzz shaping and Cairo glyph rendering.
// See Skia text_layout.cpp for the expected behavior.
#include <artist/text_layout.hpp>
#include <artist/canvas.hpp>
#include <memory>
#include <string>

namespace cycfi::artist
{
   class text_layout::impl
   {
   public:
      // TODO(cairo): Implement using HarfBuzz + Cairo glyph API in Stage 5.
      // For now only store the text so callers that read it back don't crash.
      std::u32string _text;
   };

   text_layout::text_layout(font_descr /*font_*/, std::string_view utf8)
    : _impl(std::make_unique<impl>())
   {
      // Convert UTF-8 to UTF-32 so text() returns valid data.
      // Minimal byte-by-byte ASCII approximation; multi-byte chars become '?'.
      // TODO(cairo): Replace with proper UTF-8 decode in Stage 5.
      for (unsigned char c : utf8)
         _impl->_text += (c < 0x80) ? char32_t(c) : U'?';
   }

   text_layout::text_layout(font_descr /*font_*/, std::u32string_view utf32)
    : _impl(std::make_unique<impl>())
   {
      _impl->_text = std::u32string(utf32);
   }

   text_layout::text_layout(text_layout&& rhs) noexcept
    : _impl(std::move(rhs._impl))
   {
   }

   text_layout::~text_layout()
   {
   }

   void text_layout::text(std::string_view utf8)
   {
      if (!_impl) _impl = std::make_unique<impl>();
      _impl->_text.clear();
      for (unsigned char c : utf8)
         _impl->_text += (c < 0x80) ? char32_t(c) : U'?';
   }

   void text_layout::text(std::u32string_view utf32)
   {
      if (!_impl) _impl = std::make_unique<impl>();
      _impl->_text = std::u32string(utf32);
   }

   std::u32string_view text_layout::text() const
   {
      if (!_impl) return {};
      return _impl->_text;
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
      // TODO(cairo): Stub. Returns 0 to prevent out-of-bounds crashes in callers.
      // Real implementation needed in Stage 5.
      return 0;
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
