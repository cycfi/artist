/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#if !defined(ARTIST_IMAGE_SEPTEMBER_5_2016)
#define ARTIST_IMAGE_SEPTEMBER_5_2016

#include <artist/point.hpp>
#include <artist/resources.hpp>
#include <infra/support.hpp>
#include <string_view>
#include <memory>

#if defined(ARTIST_CAIRO)
#include <cairo.h>
#endif

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
#endif

#if defined(ARTIST_CAIRO)
   using image_impl = cairo_surface_t;
#else
   class image_impl;
#endif

   using image_impl_ptr = image_impl*;

   ////////////////////////////////////////////////////////////////////////////
   // image
   ////////////////////////////////////////////////////////////////////////////
   class image : non_copyable
   {
   public:

      explicit          image(float sizex, float sizey);
      explicit          image(extent size);
      explicit          image(fs::path const& path_);
                        image(image&& rhs) noexcept;
                        ~image();

      image&            operator=(image&& rhs) noexcept;

      image_impl_ptr    impl() const;
      extent            size() const;
      void              save_png(std::string_view path) const;

      uint32_t*         pixels();
      uint32_t const*   pixels() const;
      extent            bitmap_size() const;

   private:

      image_impl_ptr  _impl;
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
