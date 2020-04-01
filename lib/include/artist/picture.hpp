/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#if !defined(ELEMENTS_IMAGE_SEPTEMBER_5_2016)
#define ELEMENTS_IMAGE_SEPTEMBER_5_2016

#include <artist/point.hpp>
#include <artist/resources.hpp>
#include <string_view>
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

   using canvas_impl_ptr = canvas_impl*;

   class picture_impl;
   using picture_impl_ptr = picture_impl*;

   ////////////////////////////////////////////////////////////////////////////
   // picture
   ////////////////////////////////////////////////////////////////////////////
   class picture
   {
   public:

      explicit          picture(float sizex, float sizey);
      explicit          picture(extent size);
      explicit          picture(fs::path const& path_);
                        picture(picture const& rhs) = delete;
                        picture(picture&& rhs) noexcept;
                        ~picture();

      picture&          operator=(picture const& rhs) = delete;
      picture&          operator=(picture&& rhs) noexcept;

      picture_impl_ptr  impl() const;
      extent            size() const;
      void              save_png(std::string_view path) const;

      uint32_t*         pixels();
      uint32_t const*   pixels() const;
      extent            bitmap_size() const;

   private:

      picture_impl_ptr  _impl;
   };

   using pixmap_ptr = std::shared_ptr<picture>;

   ////////////////////////////////////////////////////////////////////////////
   // picture_context allows drawing into a picture
   ////////////////////////////////////////////////////////////////////////////
   class picture_context
   {
   public:

      explicit          picture_context(picture& pict);
                        ~picture_context();
      picture_context&   operator=(picture_context const& rhs) = delete;

      canvas_impl_ptr   context() const;

   private:
                        picture_context(picture_context const&) = delete;

      struct state;

      picture&          _picture;
      state*            _state = nullptr;
   };

   ////////////////////////////////////////////////////////////////////////////
   // Inlines
   ////////////////////////////////////////////////////////////////////////////
   inline picture::picture(float sizex, float sizey)
    : picture(extent{ sizex, sizey })
   {
   }

   inline picture::picture(picture&& rhs) noexcept
    : _impl(std::move(rhs._impl))
   {
      rhs._impl = nullptr;
   }

   inline picture& picture::operator=(picture&& rhs) noexcept
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
