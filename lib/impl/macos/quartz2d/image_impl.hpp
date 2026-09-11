/*=============================================================================
   Copyright (c) 2016-2023 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#if !defined(ARTIST_QUARTZ2D_IMAGE_IMPL_HPP)
#define ARTIST_QUARTZ2D_IMAGE_IMPL_HPP

#include <artist/image.hpp>
#include <Quartz/Quartz.h>
#include <vector>
#include <stdexcept>

namespace cycfi::artist
{
   // Every image owns a premultiplied-BGRA CGBitmapContext from construction,
   // the same memory layout as Cairo and Skia (byte order little-endian +
   // alpha-premultiplied-first gives B,G,R,A in memory). size() is logical
   // units; bitmap_size() is pixels; scale() is pixels per logical unit.
   class image_impl
   {
   public:

      image_impl(int w, int h, float scale)
       : _width{w}, _height{h}, _scale{scale}, _buffer(size_t(w) * h * 4, 0)
      {
         _space = CGColorSpaceCreateDeviceRGB();
         _ctx = CGBitmapContextCreate(
            _buffer.data(), w, h, 8, size_t(w) * 4, _space,
            CGBitmapInfo(uint32_t(kCGBitmapByteOrder32Little) | uint32_t(kCGImageAlphaPremultipliedFirst)));
         if (!_ctx)
            throw std::runtime_error{"artist quartz2d backend: Failed to create image context."};
      }

      ~image_impl()
      {
         if (_ctx) CGContextRelease(_ctx);
         if (_space) CGColorSpaceRelease(_space);
      }

      image_impl(image_impl const&) = delete;
      image_impl& operator=(image_impl const&) = delete;

      int            width() const { return _width; }
      int            height() const { return _height; }
      float          scale() const { return _scale; }
      CGContextRef   ctx() const { return _ctx; }
      uint32_t*      pixels() { return reinterpret_cast<uint32_t*>(_buffer.data()); }
      CGImageRef     make_cgimage() const { return CGBitmapContextCreateImage(_ctx); }

   private:

      int                     _width;
      int                     _height;
      float                   _scale;
      std::vector<uint8_t>    _buffer;
      CGContextRef            _ctx = nullptr;
      CGColorSpaceRef         _space = nullptr;
   };
}

#endif
