/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#if !defined(ELEMENTS_IMAGE_SEPTEMBER_5_2016)
#define ELEMENTS_IMAGE_SEPTEMBER_5_2016

#include <artist/point.hpp>
#include <string_view>
#include <memory>

#if defined(ARTIST_SKIA)
class SkCanvas;
using host_context = SkCanvas;
#endif

namespace cycfi::artist
{
#if defined(ARTIST_QUARTZ_2D)
   struct host_context;
#endif

   using host_context_ptr = host_context*;

   class host_picture;
   using host_picture_ptr = host_picture*;

   ////////////////////////////////////////////////////////////////////////////
   // pixmaps
   ////////////////////////////////////////////////////////////////////////////
   class picture
   {
   public:

      explicit          picture(point size);
      explicit          picture(std::string_view path);
                        picture(picture const& rhs) = delete;
                        picture(picture&& rhs) noexcept;
                        ~picture();

      picture&          operator=(picture const& rhs) = delete;
      picture&          operator=(picture&& rhs) noexcept;

      host_picture_ptr  host_picture() const;
      extent            size() const;
      void              save_png(std::string_view path) const;

      uint32_t*         pixels();
      uint32_t const*   pixels() const;
      extent            bitmap_size() const;

   private:

      host_picture_ptr  _host;
   };

   using pixmap_ptr = std::shared_ptr<picture>;

   ////////////////////////////////////////////////////////////////////////////
   // pixmap_context allows drawing into a pixmap
   ////////////////////////////////////////////////////////////////////////////
   class picture_context
   {
   public:

      explicit          picture_context(picture& pict);
                        ~picture_context();
      picture_context&   operator=(picture_context const& rhs) = delete;

      host_context_ptr  context() const;

   private:
                        picture_context(picture_context const&) = delete;

      struct state;

      picture&          _picture;
      state*            _state = nullptr;
   };

   ////////////////////////////////////////////////////////////////////////////
   // Inlines
   ////////////////////////////////////////////////////////////////////////////
   inline picture::picture(picture&& rhs) noexcept
    : _host(std::move(rhs._host))
   {
      rhs._host = nullptr;
   }

   inline picture& picture::operator=(picture&& rhs) noexcept
   {
      if (this != &rhs)
      {
         _host = std::move(rhs._host);
         rhs._host = nullptr;
      }
      return *this;
   }
}

#endif
