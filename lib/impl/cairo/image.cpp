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

   image::image(uint8_t const* /*data*/, pixel_format /*fmt*/, extent /*size*/)
    : _impl(nullptr)
   {
      // TODO(cairo): Implement make_image pixel-buffer constructor.
      // See Skia implementation for expected behavior.
      throw std::runtime_error{"artist cairo backend: make_image is not yet implemented."};
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
      // TODO(cairo): Does not account for device scale (HiDPI). See Stage 8.
      return size();
   }

   size_t image::_pixmap_size(pixel_format /*fmt*/, extent /*size*/)
   {
      // TODO(cairo): Needed by make_image — implement alongside the pixel-buffer constructor.
      return 0;
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
