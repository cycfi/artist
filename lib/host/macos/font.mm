/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <canvas/font.hpp>
#include <Quartz/Quartz.h>
#include <algorithm>
#include <sstream>
#include <cmath>

namespace cycfi { namespace elements
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
    : _ptr(nullptr)
   {
   }

   font::font(font_descr descr)
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
      CFDictionaryRef font_attributes = nullptr;

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
            CFStringRef keys[] = { kCTFontAttributeName, kCTForegroundColorFromContextAttributeName };
            CFTypeRef   values[] = { (__bridge const void*)font, kCFBooleanTrue };

            font_attributes = CFDictionaryCreate(
               kCFAllocatorDefault, (const void**)&keys,
               (const void**)&values, sizeof(keys) / sizeof(keys[0]),
               &kCFTypeDictionaryKeyCallBacks,
               &kCFTypeDictionaryValueCallBacks
            );
            break;
         }
      }
      if (font_attributes)
         _ptr = (host_font_ptr) font_attributes;
   }

   font::font(font const& rhs)
    : _ptr((host_font_ptr) CFRetain(rhs._ptr))
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
         _ptr = (host_font_ptr) CFRetain(rhs._ptr);
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
}}


