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
   struct pixmap::state
   {
      NSImage* image = nullptr;
   };

   namespace
   {
      NSImage* get_ns_image(host_pixmap_ptr image)
      {
         return (__bridge NSImage*)image;
      }

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

   pixmap::pixmap(point size)
    : _state(std::make_unique<state>())
   {
      _state->image = [[NSImage alloc] initWithSize : NSMakeSize(size.x, size.y)];
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
      [_state->image addRepresentation : rep];
      [_state->image setFlipped : YES];
   }

   pixmap::pixmap(std::string_view path_)
    : _state(std::make_unique<state>())
   {
      auto path = [NSString stringWithUTF8String : std::string{path_}.c_str() ];
      _state->image = [[NSImage alloc] initWithContentsOfFile : path];
      [_state->image setFlipped : YES];
   }

   pixmap::~pixmap()
   {}

   pixmap::pixmap(pixmap&& rhs) noexcept
    : _state(std::forward<state_ptr>(rhs._state))
   {
      rhs._state = nullptr;
   }

   pixmap& pixmap::operator=(pixmap&& rhs) noexcept
   {
      _state = std::move(rhs._state);
      rhs._state = nullptr;
      return *this;
   }

   inline host_pixmap_ptr pixmap::host_pixmap() const
   {
      return (__bridge host_pixmap_ptr)_state->image;
   }

   extent pixmap::size() const
   {
      auto size_ = [_state->image size];
      return { float(size_.width), float(size_.height) };
   }

   void pixmap::save_png(std::string_view path_) const
   {
      auto path = [NSString stringWithUTF8String : std::string{path_}.c_str() ];
      auto ref = [_state->image CGImageForProposedRect : nullptr
                                               context : nullptr
                                                 hints : nullptr];
      auto* rep = [[NSBitmapImageRep alloc] initWithCGImage : ref];
      [rep setSize:[_state->image size]];

      auto* data = [rep representationUsingType : NSBitmapImageFileTypePNG properties : @{}];
      [data writeToFile : path atomically : YES];
      return;
   }

   uint32_t* pixmap::pixels()
   {
      return get_pixels(_state->image);
   }

   uint32_t const* pixmap::pixels() const
   {
      return get_pixels(_state->image);
   }

   struct pixmap_context::state
   {
      NSGraphicsContext* ctx = nullptr;
   };

   pixmap_context::pixmap_context(pixmap& pixmap_)
    : _pixmap(pixmap_)
   {
      if (auto rep = get_bitmap(get_ns_image(_pixmap.host_pixmap())))
      {
         _state = std::make_unique<state>();
         _state->ctx = [NSGraphicsContext graphicsContextWithBitmapImageRep : rep];
         [NSGraphicsContext saveGraphicsState];
         [NSGraphicsContext setCurrentContext : _state->ctx];
      }
   }

   pixmap_context::~pixmap_context()
   {
      if (_state)
      {
         [_state->ctx flushGraphics];
         [NSGraphicsContext restoreGraphicsState];
      }
   }

   host_context_ptr pixmap_context::context() const
   {
      if (_state)
      {
         auto ctx = NSGraphicsContext.currentContext.CGContext;
         return (host_context_ptr) ctx;
      }
      return nullptr;
   }
}

