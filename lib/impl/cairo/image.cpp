/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <artist/image.hpp>
#include <stdexcept>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_PNG 1
#include <artist/detail/stb_image.h>

namespace cycfi::artist
{
   image::image(extent size)
    : _impl(cairo_image_surface_create(CAIRO_FORMAT_ARGB32, size.x, size.y))
   {
      if (!_impl)
         throw std::runtime_error{ "Failed to create pixmap." };

      // Flag the surface as dirty
      cairo_surface_mark_dirty(_impl);
   }

   image::image(fs::path const& path_)
    : _impl(nullptr)
   {
      std::string full_path = find_file(path_);
      if (full_path == "")
         throw std::runtime_error{ "File does not exist:" + path_.string() };

      auto ext = path_.extension();
      if (ext == ".png" || ext == ".PNG")
      {
         // For PNGs, use Cairo's native PNG loader
         _impl = cairo_image_surface_create_from_png(full_path.c_str());
      }
      else
      {
         // For everything else, use stb_image
         int w, h, components;
         uint8_t* src_data = stbi_load(full_path.c_str(), &w, &h, &components, 4);

         if (src_data)
         {
            _impl = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);

            uint8_t* dest_data = cairo_image_surface_get_data(_impl);
            size_t   src_stride = w * 4;
            size_t   dest_stride = cairo_image_surface_get_stride(_impl);

            for (int y = 0; y != h; ++y)
            {
               uint8_t* src = src_data + (y * src_stride);
               uint8_t* dest = dest_data + (y * dest_stride);
               for (int x = 0; x != w; ++x)
               {
                  // $$$ TODO: Deal with endian issues $$$

                  dest[0] = src[2];    // blue
                  dest[1] = src[1];    // green
                  dest[2] = src[0];    // red
                  dest[3] = src[3];    // alpha

                  src += 4;
                  dest += 4;
               }
            }

            stbi_image_free(src_data);
         }
      }

      if (!_impl)
         throw std::runtime_error{ "Failed to load pixmap: " + path_.string() };

      // Flag the surface as dirty
      cairo_surface_mark_dirty(_impl);
   }

   image::~image()
   {
      if (_impl)
         cairo_surface_destroy(_impl);
   }

   image_impl_ptr image::impl() const
   {
      return _impl;
   }

   extent image::size() const
   {
      return {
         float(cairo_image_surface_get_width(_impl))
       , float(cairo_image_surface_get_height(_impl))
      };
   }

   void image::save_png(std::string_view path_) const
   {
   }

   uint32_t* image::pixels()
   {
      return nullptr;
   }

   uint32_t const* image::pixels() const
   {
      return nullptr;
   }

   extent image::bitmap_size() const
   {
      return {};
   }

   offscreen_image::offscreen_image(image& img)
    : _image(img)
   {
   }

   offscreen_image::~offscreen_image()
   {
   }

   canvas_impl* offscreen_image::context() const
   {
      return nullptr;
   }
}

