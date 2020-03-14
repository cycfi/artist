/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#if !defined(ELEMENTS_IMAGE_SEPTEMBER_5_2016)
#define ELEMENTS_IMAGE_SEPTEMBER_5_2016

#include <canvas/point.hpp>
#include <string_view>
#include <memory>

namespace cycfi::elements
{
   struct host_context;
   using host_context_ptr = host_context*;

   class host_pixmap;
   using host_pixmap_ptr = host_pixmap*;

   ////////////////////////////////////////////////////////////////////////////
   // pixmaps
   ////////////////////////////////////////////////////////////////////////////
   class pixmap
   {
   public:

      explicit          pixmap(point size);
      explicit          pixmap(std::string_view path);
                        pixmap(pixmap const& rhs) = delete;
                        pixmap(pixmap&& rhs) noexcept;
                        ~pixmap();

      pixmap&           operator=(pixmap const& rhs) = delete;
      pixmap&           operator=(pixmap&& rhs) noexcept;

      host_pixmap_ptr   host_pixmap() const;
      extent            size() const;
      void              save_png(std::string_view path) const;

      uint32_t*         pixels();
      uint32_t const*   pixels() const;

   private:

      struct state;
      using state_ptr = std::unique_ptr<state>;

      state_ptr         _state;
   };

   using pixmap_ptr = std::shared_ptr<pixmap>;

   ////////////////////////////////////////////////////////////////////////////
   // pixmap_context allows drawing into a pixmap
   ////////////////////////////////////////////////////////////////////////////
   class pixmap_context
   {
   public:

      explicit          pixmap_context(pixmap& pixmap_);
                        ~pixmap_context();
      pixmap_context&   operator=(pixmap_context const& rhs) = delete;

      host_context_ptr  context() const;

   private:
                        pixmap_context(pixmap_context const&) = delete;

      struct state;
      using state_ptr = std::unique_ptr<state>;

      pixmap&           _pixmap;
      state_ptr         _state;
   };
}

#endif
