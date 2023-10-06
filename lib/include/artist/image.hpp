/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#if !defined(ARTIST_IMAGE_SEPTEMBER_5_2016)
#define ARTIST_IMAGE_SEPTEMBER_5_2016

#include <artist/point.hpp>
#include <artist/resources.hpp>
#include <string_view>
#include <cstdint>
#include <memory>

#if defined(ARTIST_SKIA)
class SkCanvas;
using canvas_impl = SkCanvas;
#endif

namespace cycfi::artist
{
#if defined(ARTIST_QUARTZ_2D)
   struct canvas_impl;
#endif

   class image_impl;
   using image_impl_ptr = image_impl*;

   enum class pixel_format
   {
      invalid = -1,
      gray8,
      rgb16,
      rgb32,            // First byte is Alpha of 1, or ignored
      rgba32,
   };

   ////////////////////////////////////////////////////////////////////////////
   // image
   ////////////////////////////////////////////////////////////////////////////
   class image
   {
   public:

      explicit          image(float sizex, float sizey);
      explicit          image(extent size);
      explicit          image(fs::path const& path_);

                        image(image const& rhs) = delete;
                        image(image&& rhs) noexcept;
                        ~image();

      image&            operator=(image const& rhs) = delete;
      image&            operator=(image&& rhs) noexcept;

      image_impl_ptr    impl() const;
      extent            size() const;
      void              save_png(std::string_view path) const;

      uint32_t*         pixels();
      uint32_t const*   pixels() const;
      extent            bitmap_size() const;

   private:

      template <pixel_format fmt>
      friend typename std::enable_if<(fmt == pixel_format::gray8), image>::type
      make_image(std::uint8_t* data, extent size);

      template <pixel_format fmt>
      friend typename std::enable_if<(fmt == pixel_format::rgb16), image>::type
      make_image(std::uint16_t* data, extent size);

      template <pixel_format fmt>
      friend typename std::enable_if<
         (fmt == pixel_format::rgb32 || fmt == pixel_format::rgba32), image>::type
      make_image(std::uint32_t* data, extent size);

      explicit          image(std::uint8_t* data, pixel_format fmt, extent size);
      size_t            _pixmap_size(pixel_format, extent size);

      image_impl_ptr    _impl;
   };

   using image_ptr = std::shared_ptr<image>;

   ////////////////////////////////////////////////////////////////////////////
   // offscreen_image allows drawing into a picture
   ////////////////////////////////////////////////////////////////////////////
   class offscreen_image
   {
   public:

      explicit          offscreen_image(image& img);
                        ~offscreen_image();
      offscreen_image&  operator=(offscreen_image const& rhs) = delete;

      canvas_impl*      context() const;

   private:
                        offscreen_image(offscreen_image const&) = delete;

      struct state;

      image&            _image;
      state*            _state = nullptr;
   };

   ////////////////////////////////////////////////////////////////////////////
   // Inlines
   ////////////////////////////////////////////////////////////////////////////
   template <pixel_format fmt>
   inline typename std::enable_if<(fmt == pixel_format::gray8), image>::type
   make_image(std::uint8_t* data, extent size)
   {
      return image(data, pixel_format::gray8, size);
   }

   template <pixel_format fmt>
   inline typename std::enable_if<(fmt == pixel_format::rgb16), image>::type
   make_image(std::uint16_t* data, extent size)
   {
      return image(reinterpret_cast<std::uint8_t*>(data), pixel_format::rgb16, size);
   }

   template <pixel_format fmt>
   inline typename std::enable_if<
      (fmt == pixel_format::rgb32 || fmt == pixel_format::rgba32), image>::type
   make_image(std::uint32_t* data, extent size)
   {
      return image(reinterpret_cast<std::uint8_t*>(data), pixel_format::rgb32, size);
   }

   inline image::image(float sizex, float sizey)
    : image(extent{sizex, sizey })
   {
   }

   inline image::image(image&& rhs) noexcept
    : _impl(std::move(rhs._impl))
   {
      rhs._impl = nullptr;
   }

   inline image& image::operator=(image&& rhs) noexcept
   {
      if (this != &rhs)
      {
         _impl = std::move(rhs._impl);
         rhs._impl = nullptr;
      }
      return *this;
   }
}

#endif
