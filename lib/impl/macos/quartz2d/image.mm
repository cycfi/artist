/*=============================================================================
   Copyright (c) 2016-2023 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include "image_impl.hpp"
#include <Quartz/Quartz.h>
#include <ImageIO/ImageIO.h>
#include <string>
#include <vector>
#include <stdexcept>

namespace cycfi::artist
{
   image::image(extent size, float scale)
    : _impl{new image_impl(int(size.x * scale + 0.5f), int(size.y * scale + 0.5f), scale)}
   {}

   float image::scale() const
   {
      return _impl ? _impl->scale() : 1.0f;
   }

   image::image(fs::path const& path_)
    : _impl{nullptr}
   {
      auto fs_path = find_file(path_);
      auto fail = [&path_]()
      {
         throw std::runtime_error{"artist quartz2d backend: Failed to load file: " + path_.string()};
      };
      if (fs_path.empty())
         fail();

      auto url = CFURLCreateFromFileSystemRepresentation(
         nullptr, reinterpret_cast<UInt8 const*>(fs_path.c_str()), fs_path.string().size(), false);
      if (!url) fail();
      CGImageSourceRef src = CGImageSourceCreateWithURL(url, nullptr);
      CFRelease(url);
      if (!src) fail();
      CGImageRef cg = CGImageSourceCreateImageAtIndex(src, 0, nullptr);
      CFRelease(src);
      if (!cg) fail();

      int w = int(CGImageGetWidth(cg));
      int h = int(CGImageGetHeight(cg));
      _impl = new image_impl(w, h, 1.0f);
      // Default (unflipped) bitmap context: a CGImage drawn at the origin lands
      // with its top row in buffer row 0, matching pixels() on the other backends.
      CGContextDrawImage(_impl->ctx(), CGRectMake(0, 0, w, h), cg);
      CGImageRelease(cg);
   }

   image::image(uint8_t const* data, pixel_format fmt, extent size)
    : _impl{nullptr}
   {
      int w = int(size.x);
      int h = int(size.y);
      _impl = new image_impl(w, h, 1.0f);
      uint8_t* dst = reinterpret_cast<uint8_t*>(_impl->pixels());
      // Convert each source format to premultiplied BGRA, always a copy.
      switch (fmt)
      {
         case pixel_format::gray8:
            for (int i = 0; i != w * h; ++i)
            {
               uint8_t g = data[i];
               dst[i*4+0] = g; dst[i*4+1] = g; dst[i*4+2] = g; dst[i*4+3] = 0xff;
            }
            break;

         case pixel_format::rgb16:
         {
            auto const* s = reinterpret_cast<uint16_t const*>(data);
            for (int i = 0; i != w * h; ++i)
            {
               uint8_t r = uint8_t(((s[i] >> 11) & 0x1f) * 255 / 31);
               uint8_t g = uint8_t(((s[i] >>  5) & 0x3f) * 255 / 63);
               uint8_t b = uint8_t( (s[i]        & 0x1f) * 255 / 31);
               dst[i*4+0] = b; dst[i*4+1] = g; dst[i*4+2] = r; dst[i*4+3] = 0xff;
            }
            break;
         }
         case pixel_format::rgb32:
            for (int i = 0; i != w * h; ++i)
            {
               dst[i*4+0] = data[i*4+2]; dst[i*4+1] = data[i*4+1];
               dst[i*4+2] = data[i*4+0]; dst[i*4+3] = 0xff;
            }
            break;

         case pixel_format::rgba32:
            for (int i = 0; i != w * h; ++i)
            {
               uint8_t a = data[i*4+3];
               dst[i*4+0] = uint8_t((uint32_t(data[i*4+2]) * a + 127) / 255);
               dst[i*4+1] = uint8_t((uint32_t(data[i*4+1]) * a + 127) / 255);
               dst[i*4+2] = uint8_t((uint32_t(data[i*4+0]) * a + 127) / 255);
               dst[i*4+3] = a;
            }
            break;

         default:
            delete _impl; _impl = nullptr;
            throw std::runtime_error{"artist quartz2d backend: make_image: invalid pixel format."};
      }
   }

   image::~image()
   {
      delete _impl;
   }

   image_impl_ptr image::impl() const
   {
      return _impl;
   }

   extent image::size() const
   {
      if (!_impl) return {};
      float s = _impl->scale();
      return {float(_impl->width()) / s, float(_impl->height()) / s};
   }

   void image::save_png(std::string_view path_) const
   {
      auto fail = [&]()
      {
         throw std::runtime_error{"artist quartz2d backend: Failed to save file: " + std::string{path_}};
      };
      if (!_impl) fail();

      CGImageRef cg = _impl->make_cgimage();
      if (!cg) fail();

      std::string p{path_};
      auto url = CFURLCreateFromFileSystemRepresentation(
         nullptr, reinterpret_cast<UInt8 const*>(p.c_str()), p.size(), false);
      if (!url) { CGImageRelease(cg); fail(); }

      CGImageDestinationRef dest = CGImageDestinationCreateWithURL(url, CFSTR("public.png"), 1, nullptr);
      CFRelease(url);
      if (!dest) { CGImageRelease(cg); fail(); }

      CGImageDestinationAddImage(dest, cg, nullptr);
      bool ok = CGImageDestinationFinalize(dest);
      CFRelease(dest);
      CGImageRelease(cg);
      if (!ok) fail();
   }

   uint32_t* image::pixels()
   {
      return _impl ? _impl->pixels() : nullptr;
   }

   uint32_t const* image::pixels() const
   {
      return _impl ? _impl->pixels() : nullptr;
   }

   extent image::bitmap_size() const
   {
      if (!_impl) return {};
      return {float(_impl->width()), float(_impl->height())};
   }

   ////////////////////////////////////////////////////////////////////////////
   // offscreen_image draws directly into the image's own bitmap context: the
   // drawing is in the image immediately and earlier contents are kept. The
   // flipped, scaled CTM gives draw code Artist's top-left, y-down logical
   // coordinates (what lockFocusFlipped used to provide).
   struct offscreen_image::state {};

   offscreen_image::offscreen_image(image& img)
    : _image{img}
    , _state{nullptr}
   {
      auto ctx = _image.impl()->ctx();
      CGContextSaveGState(ctx);
      CGContextTranslateCTM(ctx, 0, _image.impl()->height());
      CGContextScaleCTM(ctx, _image.impl()->scale(), -_image.impl()->scale());
   }

   offscreen_image::~offscreen_image()
   {
      CGContextRestoreGState(_image.impl()->ctx());
   }

   canvas_impl* offscreen_image::context() const
   {
      return (canvas_impl*) _image.impl()->ctx();
   }
}
