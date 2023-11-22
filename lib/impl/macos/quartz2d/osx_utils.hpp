/*=============================================================================
   Copyright (c) 2016-2023 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#if !defined(ARTIST_DETAIL_OSX_UTILS_MARCH_17_2020)
#define ARTIST_DETAIL_OSX_UTILS_MARCH_17_2020

#include <Quartz/Quartz.h>
#include <string_view>
#include <infra/support.hpp>

namespace cycfi::artist::detail
{
   inline CFStringRef cf_string(char const* f, char const* l)
   {
      return CFStringCreateWithBytesNoCopy(
         nullptr, (UInt8 const*)f, l-f, kCFStringEncodingUTF8
       , false, kCFAllocatorNull
      );
   }

   inline CFStringRef cf_string(char32_t const* f, char32_t const* l)
   {
      auto enc = is_little_endian()?
         kCFStringEncodingUTF32LE : kCFStringEncodingUTF32BE
         ;
      return CFStringCreateWithBytesNoCopy(
         nullptr, (UInt8 const*)f, (l-f)*sizeof(char32_t), enc
       , false, kCFAllocatorNull
      );
   }

   inline CFStringRef cf_string(std::string_view str)
   {
      return cf_string(str.data(), str.data()+str.size());
   }

   inline CFStringRef cf_string(std::u32string_view str)
   {
      return cf_string(str.data(), str.data()+str.size());
   }

   inline NSString* ns_string(char const* f, char const* l)
   {
      return (__bridge_transfer NSString*) cf_string(f, l);
   }

   inline NSString* ns_string(std::string_view str)
   {
      return ns_string(str.data(), str.data()+str.size());
   }
}

#endif
