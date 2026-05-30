/*=============================================================================
   Copyright (c) 2016-2024 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
// TODO(cairo): Font support is a stub. See Stage 5 for FreeType/Fontconfig integration.
// The Artist font class requires mapping font_descr to a cairo_font_face_t.
// The toy Cairo font API is not suitable for production font matching or metrics.
// See ai/artist-cairo-backend-assimilation-plan.md Stage 5.
#include <artist/font.hpp>

namespace cycfi::artist
{
   // Private font_impl for Cairo — placeholder until Stage 5.
   // font.hpp forward-declares 'struct font_impl' inside cycfi::artist.
   struct font_impl
   {
      // Will hold cairo_scaled_font_t* or cairo_font_face_t* in Stage 5.
   };

   font::font()
    : _ptr(new font_impl)
   {
   }

   font::font(font_descr /*descr*/)
    : _ptr(new font_impl)
   {
      // TODO(cairo): Map font_descr to a Cairo font face via FreeType/Fontconfig.
      // See Skia font.cpp for the expected font matching behavior.
   }

   font::font(font const& rhs)
    : _ptr(rhs._ptr)
   {
   }

   font::font(font&& rhs) noexcept
    : _ptr(std::move(rhs._ptr))
   {
   }

   font::~font()
   {
   }

   font& font::operator=(font const& rhs)
   {
      _ptr = rhs._ptr;
      return *this;
   }

   font& font::operator=(font&& rhs) noexcept
   {
      _ptr = std::move(rhs._ptr);
      return *this;
   }

   font::metrics_info font::metrics() const
   {
      // TODO(cairo): Return real metrics via cairo_scaled_font_extents in Stage 5.
      return {};
   }

   float font::measure_text(std::string_view /*str*/) const
   {
      // TODO(cairo): Implement via cairo_text_extents in Stage 5.
      return 0.0f;
   }
}
