/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#if !defined(ARTIST_DETAIL_OSX_UTILS_MARCH_17_2020)
#define ARTIST_DETAIL_OSX_UTILS_MARCH_17_2020

#include <Quartz/Quartz.h>

namespace cycfi::artist::detail
{
   inline CFStringRef cf_string(char const* f, char const* l)
   {
      return CFStringCreateWithBytesNoCopy(
         nullptr, (UInt8 const*)f, l-f, kCFStringEncodingUTF8
       , false, kCFAllocatorNull
      );
   }
}

#endif
