/*=============================================================================
   Copyright (c) 2016-2024 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <artist/image.hpp>
#include "cairo_private.hpp"
#include <stdexcept>
#include <string>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_PNG 1
#include "stb_image.h"

namespace cycfi::artist
{
   image::image(extent size)
    : _impl(new image_impl(
         cairo_image_surface_create(CAIRO_FORMAT_ARGB32,
            int(size.x), int(size.y))))
   {
      if (!_impl->surface || cairo_surface_status(_impl->surface) != CAIRO_STATUS_SUCCESS)
         throw std::runtime_error{"artist cairo backend: Failed to create image surface."};
      // No cairo_surface_mark_dirty: Cairo owns the zero-initialized pixel data.
      // mark_dirty is only needed after *external* writes to the pixel buffer.
   }

   image::image(fs::path const& path_)
    : _impl(nullptr)
   {
      std::string full_path = find_file(path_);
      if (full_path.empty())
         throw std::runtime_error{"artist cairo backend: File does not exist: " + path_.string()};

      cairo_surface_t* surface = nullptr;
      auto ext = path_.extension();

      if (ext == ".png" || ext == ".PNG")
      {
         surface = cairo_image_surface_create_from_png(full_path.c_str());
      }
      else
      {
         int w, h, components;
         uint8_t* src_data = stbi_load(full_path.c_str(), &w, &h, &components, 4);
         if (src_data)
         {
            surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
            if (surface && cairo_surface_status(surface) == CAIRO_STATUS_SUCCESS)
            {
               uint8_t* dest_data   = cairo_image_surface_get_data(surface);
               int      dest_stride = cairo_image_surface_get_stride(surface);
               int      src_stride  = w * 4;

               for (int row = 0; row != h; ++row)
               {
                  uint8_t const* src  = src_data + (row * src_stride);
                  uint8_t*       dest = dest_data + (row * dest_stride);
                  for (int col = 0; col != w; ++col)
                  {
                     // stb_image gives straight-alpha RGBA.
                     // Cairo ARGB32 (little-endian) is premultiplied BGRA in memory:
                     //   byte[0]=B, byte[1]=G, byte[2]=R, byte[3]=A
                     // Each channel must be premultiplied by alpha/255.
                     // TODO(cairo): Verify endian behavior on BE platforms.
                     uint8_t a = src[3];
                     dest[0] = uint8_t((uint32_t(src[2]) * a + 127) / 255);  // B ← R * A
                     dest[1] = uint8_t((uint32_t(src[1]) * a + 127) / 255);  // G * A
                     dest[2] = uint8_t((uint32_t(src[0]) * a + 127) / 255);  // R ← B * A
                     dest[3] = a;                                              // A
                     src  += 4;
                     dest += 4;
                  }
               }
            }
            stbi_image_free(src_data);
            if (surface && cairo_surface_status(surface) == CAIRO_STATUS_SUCCESS)
               cairo_surface_mark_dirty(surface);
         }
      }

      if (!surface || cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS)
         throw std::runtime_error{"artist cairo backend: Failed to load image: " + path_.string()};
      _impl = new image_impl(surface);
   }

   image::image(uint8_t const* data, pixel_format fmt, extent size)
    : _impl(nullptr)
   {
      if (fmt == pixel_format::invalid)
         throw std::runtime_error{"artist cairo backend: make_image: invalid pixel format."};

      int w = int(size.x);
      int h = int(size.y);
      auto surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
      if (!surface || cairo_surface_status(surface) != CAIRO_STATUS_SUCCESS)
         throw std::runtime_error{"artist cairo backend: make_image: failed to create surface."};

      uint8_t*  dest        = cairo_image_surface_get_data(surface);
      int       dest_stride = cairo_image_surface_get_stride(surface);

      // Convert each source format to Cairo ARGB32 (premultiplied BGRA, little-endian).
      switch (fmt)
      {
         case pixel_format::gray8:
         {
            for (int y = 0; y < h; ++y)
            {
               uint8_t const* src = data + y * w;
               uint8_t*       dst = dest + y * dest_stride;
               for (int x = 0; x < w; ++x, ++src, dst += 4)
               {
                  dst[0] = *src;  // B
                  dst[1] = *src;  // G
                  dst[2] = *src;  // R
                  dst[3] = 0xff;  // A
               }
            }
            break;
         }
         case pixel_format::rgb16:
         {
            // Packed RGB565: RRRRRGGGGGGBBBBB
            auto const* src16 = reinterpret_cast<uint16_t const*>(data);
            for (int y = 0; y < h; ++y)
            {
               uint16_t const* src = src16 + y * w;
               uint8_t*        dst = dest  + y * dest_stride;
               for (int x = 0; x < w; ++x, ++src, dst += 4)
               {
                  uint8_t r = uint8_t(((*src >> 11) & 0x1f) * 255 / 31);
                  uint8_t g = uint8_t(((*src >>  5) & 0x3f) * 255 / 63);
                  uint8_t b = uint8_t( (*src        & 0x1f) * 255 / 31);
                  dst[0] = b;
                  dst[1] = g;
                  dst[2] = r;
                  dst[3] = 0xff;
               }
            }
            break;
         }
         case pixel_format::rgb32:
         {
            // Straight RGBA, alpha ignored (treated as opaque).
            auto const* src32 = reinterpret_cast<uint32_t const*>(data);
            for (int y = 0; y < h; ++y)
            {
               uint32_t const* src = src32 + y * w;
               uint8_t*        dst = dest  + y * dest_stride;
               for (int x = 0; x < w; ++x, ++src, dst += 4)
               {
                  auto* s = reinterpret_cast<uint8_t const*>(src);
                  dst[0] = s[2];  // B ← source B (alpha forced opaque)
                  dst[1] = s[1];  // G
                  dst[2] = s[0];  // R
                  dst[3] = 0xff;  // A = 1
               }
            }
            break;
         }
         case pixel_format::rgba32:
         {
            // Straight RGBA → premultiplied BGRA.
            auto const* src32 = reinterpret_cast<uint32_t const*>(data);
            for (int y = 0; y < h; ++y)
            {
               uint32_t const* src = src32 + y * w;
               uint8_t*        dst = dest  + y * dest_stride;
               for (int x = 0; x < w; ++x, ++src, dst += 4)
               {
                  auto* s = reinterpret_cast<uint8_t const*>(src);
                  uint8_t a = s[3];
                  dst[0] = uint8_t((uint32_t(s[2]) * a + 127) / 255);  // B premul
                  dst[1] = uint8_t((uint32_t(s[1]) * a + 127) / 255);  // G premul
                  dst[2] = uint8_t((uint32_t(s[0]) * a + 127) / 255);  // R premul
                  dst[3] = a;
               }
            }
            break;
         }
         default:
            cairo_surface_destroy(surface);
            throw std::runtime_error{"artist cairo backend: make_image: unsupported pixel format."};
      }

      cairo_surface_mark_dirty(surface);
      _impl = new image_impl(surface);
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
      if (!_impl || !_impl->surface) return {};
      return {
         float(cairo_image_surface_get_width(_impl->surface)),
         float(cairo_image_surface_get_height(_impl->surface))
      };
   }

   void image::save_png(std::string_view path_) const
   {
      if (!_impl || !_impl->surface) return;
      std::string p{path_};
      auto status = cairo_surface_write_to_png(_impl->surface, p.c_str());
      if (status != CAIRO_STATUS_SUCCESS)
         throw std::runtime_error{
            std::string{"artist cairo backend: Failed to save PNG: "} + cairo_status_to_string(status)};
   }

   uint32_t* image::pixels()
   {
      if (!_impl || !_impl->surface) return nullptr;
      // Pixel format: premultiplied BGRA (CAIRO_FORMAT_ARGB32 on little-endian).
      // Artist public API convention is straight-alpha RGBA; callers that read or
      // write pixels must account for the format difference.
      //
      // cairo_surface_flush() synchronises Cairo's internal state to the CPU buffer
      // before the pointer is handed out.  After writing to the returned pointer,
      // callers must call cairo_surface_mark_dirty(surface) on the underlying
      // surface before using the image again — this API does not expose that call.
      cairo_surface_flush(_impl->surface);
      return reinterpret_cast<uint32_t*>(cairo_image_surface_get_data(_impl->surface));
   }

   uint32_t const* image::pixels() const
   {
      if (!_impl || !_impl->surface) return nullptr;
      cairo_surface_flush(_impl->surface);
      return reinterpret_cast<uint32_t const*>(cairo_image_surface_get_data(_impl->surface));
   }

   extent image::bitmap_size() const
   {
      // cairo_image_surface_get_width/height returns physical pixel dimensions.
      // size() uses the same functions, so bitmap_size() == size() for Cairo image
      // surfaces.  If cairo_surface_set_device_scale were applied, size() would
      // still return physical pixels (unchanged by device scale), so the two remain
      // equivalent — bitmap_size() is always in physical pixels on all backends.
      return size();
   }

   size_t image::_pixmap_size(pixel_format fmt, extent size)
   {
      size_t pixels = size_t(size.x) * size_t(size.y);
      switch (fmt)
      {
         case pixel_format::gray8:  return pixels;
         case pixel_format::rgb16:  return pixels * 2;
         case pixel_format::rgb32:
         case pixel_format::rgba32: return pixels * 4;
         default:                   return 0;
      }
   }

   ////////////////////////////////////////////////////////////////////////////
   // offscreen_image

   struct offscreen_image::state
   {
      cairo_t* cairo_ctx = nullptr;
   };

   offscreen_image::offscreen_image(image& img)
    : _image{img}
    , _state{new offscreen_image::state{}}
   {
      if (_image.impl() && _image.impl()->surface)
         _state->cairo_ctx = cairo_create(_image.impl()->surface);
   }

   offscreen_image::~offscreen_image()
   {
      if (_state->cairo_ctx)
         cairo_destroy(_state->cairo_ctx);
      delete _state;
   }

   canvas_impl* offscreen_image::context() const
   {
      return _state->cairo_ctx;
   }
}
