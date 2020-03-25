/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <artist/picture.hpp>
#include <Quartz/Quartz.h>
#include <string>
#include <stdexcept>

namespace cycfi::artist
{
   namespace
   {
      NSBitmapImageRep* get_bitmap(NSImage* image)
      {
         for (NSImageRep* rep in [image representations])
            if ([rep isKindOfClass : [NSBitmapImageRep class]])
               return (NSBitmapImageRep*)rep;
         return nullptr;
      }

      uint32_t* get_pixels(NSImage* image)
      {
         if (auto bitmap = get_bitmap(image))
            return (uint32_t*) [bitmap bitmapData];
         return nullptr;
      }
   }

   picture::picture(point size)
   {
      auto img_ = [[NSImage alloc] initWithSize : NSMakeSize(size.x, size.y)];
      _impl = (__bridge_retained picture_impl_ptr) img_;
   }

   picture::picture(std::string_view path_)
   {
      auto path = [NSString stringWithUTF8String : std::string{path_}.c_str() ];
      auto img_ = [[NSImage alloc] initWithContentsOfFile : path];
      if (!img_)
         img_ = [NSImage imageNamed : path];
      _impl = (__bridge_retained picture_impl_ptr) img_;
   }

   picture::~picture()
   {
      CFBridgingRelease(_impl);
   }

   picture_impl_ptr picture::impl() const
   {
      return _impl;
   }

   extent picture::size() const
   {
      auto size_ = [(__bridge NSImage*) _impl size];
      return { float(size_.width), float(size_.height) };
   }

   void picture::save_png(std::string_view path_) const
   {
      auto path = [NSString stringWithUTF8String : std::string{path_}.c_str() ];
      auto image = (__bridge NSImage*) _impl;
      auto ref = [image CGImageForProposedRect : nullptr
                                       context : nullptr
                                         hints : nullptr];
      auto* rep = [[NSBitmapImageRep alloc] initWithCGImage : ref];
      [rep setSize:[image size]];

      auto* data = [rep representationUsingType : NSBitmapImageFileTypePNG
                                     properties : @{}];
      [data writeToFile : path atomically : YES];
   }

   uint32_t* picture::pixels()
   {
      return get_pixels((__bridge NSImage*) _impl);
   }

   uint32_t const* picture::pixels() const
   {
      return get_pixels((__bridge NSImage*) _impl);
   }

   extent picture::bitmap_size() const
   {
      auto bm = get_bitmap((__bridge NSImage*) _impl);
      auto pixels_wide = [bm pixelsWide];
      auto pixels_high = [bm pixelsHigh];
      return { float(pixels_wide), float(pixels_high) };
   }

   picture_context::picture_context(picture& pict)
    : _picture(pict)
   {
      [((__bridge NSImage*) _picture.impl()) lockFocusFlipped : YES];
   }

   picture_context::~picture_context()
   {
      [((__bridge NSImage*) _picture.impl()) unlockFocus];
   }

   canvas_impl_ptr picture_context::context() const
   {
      return (canvas_impl_ptr) NSGraphicsContext.currentContext.CGContext;
   }
}

