/*=============================================================================
   Copyright (c) 2016-2023 Joel de Guzman

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
#elif defined(ARTIST_CAIRO)
extern "C" { typedef struct _cairo cairo_t; }
using canvas_impl = cairo_t;
#endif

namespace cycfi::artist
{
#if defined(ARTIST_QUARTZ_2D)
   struct canvas_impl;
#elif defined(ARTIST_DIRECT2D)
   namespace d2d { struct context; }
   using canvas_impl = d2d::context;
#elif !defined(ARTIST_SKIA) && !defined(ARTIST_CAIRO)
   struct canvas_impl;   // no backend selected: opaque, declaration only
#endif

   class image_impl;
   using image_impl_ptr = image_impl*;

   enum class pixel_format
   {
      invalid = -1,
      gray8,
      rgb16,
      rgb32,            // R, G, B in memory order, then an ignored byte
      rgba32,
   };

   ////////////////////////////////////////////////////////////////////////////
   // image
   ////////////////////////////////////////////////////////////////////////////
   class image
   {
   public:

      explicit          image(float sizex, float sizey, float scale = 1.0f);
      explicit          image(extent size, float scale = 1.0f);
      explicit          image(fs::path const& path_);

                        image(image const& rhs) = delete;
                        image(image&& rhs) noexcept;
                        ~image();

      image&            operator=(image const& rhs) = delete;
      image&            operator=(image&& rhs) noexcept;

      image_impl_ptr    impl() const;
      extent            size() const;
      float             scale() const;
      void              save_png(std::string_view path) const;

      // Returns a pointer to the bitmap: bitmap_size().x by bitmap_size().y
      // pixels, rows from the top with no padding. Every backend uses the
      // same layout: premultiplied B, G, R, A in memory order.
      // Cairo note: pixels() calls cairo_surface_flush() before returning the
      // pointer. If you write to the buffer, you must mark the surface dirty
      // before using the image again; this API does not expose that call.
      uint32_t*         pixels();
      uint32_t const*   pixels() const;
      extent            bitmap_size() const;

   private:

      template <pixel_format fmt>
      friend typename std::enable_if<(fmt == pixel_format::gray8), image>::type
      make_image(std::uint8_t const* data, extent size);

      template <pixel_format fmt>
      friend typename std::enable_if<(fmt == pixel_format::rgb16), image>::type
      make_image(std::uint16_t const* data, extent size);

      template <pixel_format fmt>
      friend typename std::enable_if<
         (fmt == pixel_format::rgb32 || fmt == pixel_format::rgba32), image>::type
      make_image(std::uint32_t const* data, extent size);

      explicit          image(std::uint8_t const* data, pixel_format fmt, extent size);
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
   make_image(std::uint8_t const* data, extent size)
   {
      return image(data, fmt, size);
   }

   template <pixel_format fmt>
   inline typename std::enable_if<(fmt == pixel_format::rgb16), image>::type
   make_image(std::uint16_t const* data, extent size)
   {
      return image(reinterpret_cast<std::uint8_t const*>(data), fmt, size);
   }

   template <pixel_format fmt>
   inline typename std::enable_if<
      (fmt == pixel_format::rgb32 || fmt == pixel_format::rgba32), image>::type
   make_image(std::uint32_t const* data, extent size)
   {
      return image(reinterpret_cast<std::uint8_t const*>(data), fmt, size);
   }

   inline image::image(float sizex, float sizey, float scale)
    : image(extent{sizex, sizey}, scale)
   {
   }

   inline image::image(image&& rhs) noexcept
    : _impl(std::move(rhs._impl))
   {
      rhs._impl = nullptr;
   }

   inline image& image::operator=(image&& rhs) noexcept
   {
      // Swap so the old _impl is destroyed by rhs's destructor in the backend
      // TU where image_impl is complete (it is incomplete here). A plain
      // overwrite would leak the old _impl.
      std::swap(_impl, rhs._impl);
      return *this;
   }
}

#endif
