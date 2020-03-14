/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <canvas/picture.hpp>
#include <Quartz/Quartz.h>
#include <string>
#include <stdexcept>

namespace cycfi::elements
{
   struct picture::state
   {
      NSImage* image = nullptr;
   };

   namespace
   {
      NSImage* get_ns_image(host_picture_ptr image)
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

   picture::picture(point size)
    : _state(std::make_unique<state>())
   {
      _state->image = [[NSImage alloc] initWithSize : NSMakeSize(size.x, size.y)];
   }

   picture::picture(std::string_view path_)
    : _state(std::make_unique<state>())
   {
      auto path = [NSString stringWithUTF8String : std::string{path_}.c_str() ];
      _state->image = [[NSImage alloc] initWithContentsOfFile : path];
   }

   picture::~picture()
   {}

   picture::picture(picture&& rhs) noexcept
    : _state(std::forward<state_ptr>(rhs._state))
   {
      rhs._state = nullptr;
   }

   picture& picture::operator=(picture&& rhs) noexcept
   {
      _state = std::move(rhs._state);
      rhs._state = nullptr;
      return *this;
   }

   inline host_picture_ptr picture::host_pixmap() const
   {
      return (__bridge host_picture_ptr)_state->image;
   }

   extent picture::size() const
   {
      auto size_ = [_state->image size];
      return { float(size_.width), float(size_.height) };
   }

   void picture::save_png(std::string_view path_) const
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

   uint32_t* picture::pixels()
   {
      return get_pixels(_state->image);
   }

   uint32_t const* picture::pixels() const
   {
      return get_pixels(_state->image);
   }

   struct picture_context::state
   {
      NSGraphicsContext* ctx = nullptr;
   };

   picture_context::picture_context(picture& pixmap_)
    : _picture(pixmap_)
   {
      [get_ns_image(_picture.host_pixmap()) lockFocusFlipped : YES];
   }

   picture_context::~picture_context()
   {
      [get_ns_image(_picture.host_pixmap()) unlockFocus];
   }

   host_context_ptr picture_context::context() const
   {
      return (host_context_ptr) NSGraphicsContext.currentContext.CGContext;
   }
}

