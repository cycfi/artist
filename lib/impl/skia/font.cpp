/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <artist/font.hpp>
#include <algorithm>
#include <sstream>
#include <cmath>

namespace cycfi::artist
{
   font::font()
    : _ptr(nullptr)
   {
   }

   font::font(font_descr descr)
    : _ptr(nullptr)
   {
   }

   font::font(font const& rhs)
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

   font& font::operator=(font const& rhs)
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
}


