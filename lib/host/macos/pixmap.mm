/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <canvas/pixmap.hpp>
#include <Quartz/Quartz.h>
#include <string> // $$$ meybe uneeded $$$

namespace cycfi::elements
{
   pixmap::pixmap(point size)
   {
   }

   pixmap::pixmap(std::string_view filename)
   {
      auto filename_ = [NSString stringWithUTF8String : std::string{filename}.c_str() ];
      auto img_ = [[NSImage alloc] initWithContentsOfFile : filename_];
      _pixmap = (__bridge_retained host_pixmap_ptr) img_;
   }

   pixmap::~pixmap()
   {
      CFBridgingRelease(_pixmap);
   }

   extent pixmap::size() const
   {
      auto size_ = [((__bridge NSImage*) _pixmap) size];
      return { float(size_.width), float(size_.height) };
   }

   pixmap_context::pixmap_context(pixmap& pm)
   {
   }

   pixmap_context::~pixmap_context()
   {
   }
}

