/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]

   BASELINE STUB. Real DirectWrite implementation lands in task #7
   (font_descr -> IDWriteTextFormat, metrics, measure_text).
=============================================================================*/
#include <artist/font.hpp>

namespace cycfi::artist
{
   font::font()
    : _ptr(nullptr)
   {
   }

   font::font(font_descr /*descr*/)
    : _ptr(nullptr)
   {
   }

   font::font(font const& /*rhs*/)
    : _ptr(nullptr)
   {
   }

   font::font(font&& rhs) noexcept
    : _ptr(rhs._ptr)
   {
      rhs._ptr = nullptr;
   }

   font::~font()
   {
   }

   font& font::operator=(font const& /*rhs*/)
   {
      return *this;
   }

   font& font::operator=(font&& rhs) noexcept
   {
      if (this != &rhs)
      {
         _ptr = rhs._ptr;
         rhs._ptr = nullptr;
      }
      return *this;
   }

   font::metrics_info font::metrics() const
   {
      return {};
   }

   float font::measure_text(std::string_view /*str*/) const
   {
      return 0;
   }
}
