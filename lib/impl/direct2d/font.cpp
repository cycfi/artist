/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <artist/font.hpp>

namespace cycfi::artist
{
   font::font()
   {
   }

   font::font(font_descr descr)
   {
   }

   font::font(font const& rhs)
   {
   }

   font::font(font&& rhs) noexcept
   {
   }

   font::~font()
   {
   }

   font& font::operator=(font const& rhs)
   {
      return *this;
   }

   font& font::operator=(font&& rhs) noexcept
   {
      return *this;
   }

   font::metrics_info font::metrics() const
   {
      return {};
   }

   float font::measure_text(std::string_view str) const
   {
      return -1;
   }
}


