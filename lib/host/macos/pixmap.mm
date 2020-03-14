/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <canvas/pixmap.hpp>
#include <Quartz/Quartz.h>
#include <string>
#include <stdexcept>

namespace cycfi::elements
{
   pixmap::pixmap(point size)
   {
      auto img_ = [[NSImage alloc] initWithSize : NSMakeSize(size.x, size.y)];
      NSBitmapImageRep* rep =
         [[NSBitmapImageRep alloc]
            initWithBitmapDataPlanes : nullptr
                          pixelsWide : size.x
                          pixelsHigh : size.y
                       bitsPerSample : 8
                     samplesPerPixel : 4
                            hasAlpha : YES
                            isPlanar : NO
                      colorSpaceName : NSCalibratedRGBColorSpace
                         bytesPerRow : 0
                        bitsPerPixel : 0
         ];
      [img_ addRepresentation : rep];
      _pixmap = (__bridge_retained host_pixmap_ptr) img_;
   }

   pixmap::pixmap(std::string_view path_)
   {
      auto path = [NSString stringWithUTF8String : std::string{path_}.c_str() ];
      auto img_ = [[NSImage alloc] initWithContentsOfFile : path];
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

   void pixmap::save_png(std::string_view path_) const
   {
      auto path = [NSString stringWithUTF8String : std::string{path_}.c_str() ];
      auto image = (__bridge NSImage*) _pixmap;
      auto ref = [image CGImageForProposedRect : nullptr
                                       context : nullptr
                                         hints : nullptr];
      auto* rep = [[NSBitmapImageRep alloc] initWithCGImage : ref];
      [rep setSize:[image size]];

      auto* data = [rep representationUsingType : NSBitmapImageFileTypePNG properties : @{}];
      [data writeToFile : path atomically : YES];
      return;
   }

   namespace
   {
      uint32_t* get_pixels(NSImage* image)
      {
         for (NSImageRep* rep in [image representations])
            if ([rep isKindOfClass : [NSBitmapImageRep class]])
               return (uint32_t*) [((NSBitmapImageRep*)rep) bitmapData];
         return nullptr;
      }
   }

   uint32_t* pixmap::pixels()
   {
      return get_pixels((__bridge NSImage*) _pixmap);
   }

   uint32_t const* pixmap::pixels() const
   {
      return get_pixels((__bridge NSImage*) _pixmap);
   }

   pixmap_context::pixmap_context(pixmap& pixmap_)
    : _pixmap(pixmap_)
   {
      [((__bridge NSImage*) _pixmap.host_pixmap()) lockFocusFlipped : YES];
   }

   pixmap_context::~pixmap_context()
   {
      [((__bridge NSImage*) _pixmap.host_pixmap()) unlockFocus];
   }

   host_context_ptr pixmap_context::context() const
   {
      return (host_context_ptr) NSGraphicsContext.currentContext.CGContext;
   }
}

