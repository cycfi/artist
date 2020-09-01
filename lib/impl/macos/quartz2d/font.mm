/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <artist/font.hpp>
#include <Quartz/Quartz.h>
#include <algorithm>
#include <sstream>
#include <cmath>
#include "osx_utils.hpp"

namespace cycfi::artist
{
   namespace
   {
      inline void ltrim(std::string& s)
      {
         s.erase(s.begin(), std::find_if(s.begin(), s.end(),
            [](int ch) { return ch != ' ' && ch != '"'; }
         ));
      }

      inline void rtrim(std::string& s)
      {
         s.erase(std::find_if(s.rbegin(), s.rend(),
            [](int ch) { return ch != ' ' && ch != '"'; }
         ).base(), s.end());
      }

      inline void trim(std::string& s)
      {
         ltrim(s);
         rtrim(s);
      }
   }

   using namespace font_constants;

   font::font()
    : _ptr(nullptr)
   {
   }

   font::font(font_descr descr)
    : _ptr(nullptr)
   {
      int weight = std::ceil(float(descr._weight * 5) / 40);
      int style = 0;

      if (descr._slant)
         style |= NSItalicFontMask;
      if (descr._stretch <= condensed)
         style |= NSCondensedFontMask;
      else if (descr._stretch >= expanded)
         style |= NSExpandedFontMask;

      std::istringstream str(std::string{ descr._families });
      std::string family;
      auto font_manager = [NSFontManager sharedFontManager];

      while (getline(str, family, ','))
      {
         trim(family);
         auto  family_ = [NSString stringWithUTF8String : family.c_str()];

         auto font =
            [font_manager
               fontWithFamily : family_
                       traits : style
                       weight : weight
                         size : descr._size
            ];

         if (font)
         {
            _ptr = (__bridge font_impl_ptr) font;
            CFRetain(_ptr);
            break;
         }
      }
   }

   font::font(font const& rhs)
    : _ptr((font_impl_ptr) CFRetain(rhs._ptr))
   {
   }

   font::font(font&& rhs) noexcept
    : _ptr(rhs._ptr)
   {
      rhs._ptr = nullptr;
   }

   font::~font()
   {
      if (_ptr)
         CFRelease(_ptr);
   }

   font& font::operator=(font const& rhs)
   {
      if (this != &rhs)
         _ptr = (font_impl_ptr) CFRetain(rhs._ptr);
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
      NSFont* font = (__bridge NSFont*) _ptr;
      return {
         float([font ascender]),
         float(-[font descender]),
         float([font leading])
      };
   }

   float font::measure_text(std::string_view str) const
   {
      NSFont* font = (__bridge NSFont*) _ptr;
      NSDictionary* attr = @{ NSFontAttributeName : font };
      NSString* text = detail::ns_string(str);
      const CGSize textSize = [text sizeWithAttributes : attr];
      return textSize.width;
   }
}


