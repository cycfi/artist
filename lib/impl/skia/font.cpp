/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <artist/font.hpp>
#include <SkTypeface.h>
#include <SkFont.h>
#include <sstream>

namespace cycfi::artist
{
   namespace
   {
      inline void ltrim(std::string& s)
      {
         s.erase(s.begin(), std::find_if(s.begin(), s.end(),
            [](int ch) { return !std::isspace(ch); }
         ));
      }

      inline void rtrim(std::string& s)
      {
         s.erase(std::find_if(s.rbegin(), s.rend(),
            [](int ch) { return !std::isspace(ch); }
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
    : _ptr(SkFont::Make(nullptr, 12, SkFont::kA8_MaskType, 0).get())
   {
      if (_ptr)
         _ptr->ref();
   }

   font::font(font_descr descr)
   {
      int stretch = int(descr._stretch) / 10;
      SkFontStyle style(
         descr._weight * 10
       , (descr._stretch < condensed)? stretch-1 : stretch
       , (descr._slant == italic)? SkFontStyle::kItalic_Slant :
         (descr._slant == oblique)? SkFontStyle::kOblique_Slant :
         SkFontStyle::kUpright_Slant
      );

      auto default_face = SkTypeface::MakeFromName(nullptr, style);
      std::istringstream str(std::string{ descr._families });
      std::string family;

      while (getline(str, family, ','))
      {
         trim(family);
         auto face = SkTypeface::MakeFromName(family.c_str(), style);
         if (face != default_face)
         {
            if (auto font = SkFont::Make(face, descr._size, SkFont::kA8_MaskType, 0))
            {
               _ptr = font.get();
               _ptr->ref();
            }
            break;
         }
      }
      if (!_ptr)
      {
         auto font = SkFont::Make(default_face, descr._size, SkFont::kA8_MaskType, 0);
         _ptr = font.get();
         _ptr->ref();
      }
   }

   font::font(font const& rhs)
   {
      if (rhs._ptr)
      {
         _ptr = rhs._ptr;
         _ptr->ref();
      }
   }

   font::font(font&& rhs) noexcept
    : _ptr(rhs._ptr)
   {
      rhs._ptr = nullptr;
   }

   font::~font()
   {
      if (_ptr)
         _ptr->unref();
   }

   font& font::operator=(font const& rhs)
   {
      if (this != &rhs)
      {
         _ptr = rhs._ptr;
         _ptr->ref();
      }
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


