/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <artist/font.hpp>
#include <SkTypeface.h>
#include <SkFont.h>
#include <sstream>
#include <SkFontMetrics.h>

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
    : _ptr(std::make_shared<SkFont>())
   {
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
            _ptr = std::make_shared<SkFont>(face, descr._size);
            break;
         }
      }
      if (!_ptr)
         _ptr = std::make_shared<SkFont>(default_face, descr._size);
   }

   font::font(font const& rhs)
   {
      _ptr = rhs._ptr;
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
      SkFontMetrics sk_metrics;
      _ptr->getMetrics(&sk_metrics);
      return { -sk_metrics.fAscent, sk_metrics.fDescent, sk_metrics.fLeading };
   }

   float font::measure_text(std::string_view str) const
   {
      return _ptr->measureText(str.data(), str.size(), SkTextEncoding::kUTF8);
   }
}


