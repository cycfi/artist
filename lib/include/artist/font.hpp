/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#if !defined(ARTIST_FONT_FEBRUARY_11_2020)
#define ARTIST_FONT_FEBRUARY_11_2020

#include <string_view>
#include <infra/filesystem.hpp>
#include <memory>

#if defined(ARTIST_SKIA)
class SkFont;
#endif

namespace cycfi::artist
{
#if defined(ARTIST_SKIA)
   using font_impl = SkFont;
   using font_impl_ptr = std::shared_ptr<SkFont>;
#else
   struct font_impl;
   using font_impl_ptr = font_impl*;
#endif

   namespace font_constants
   {
      enum weight_enum
      {
         thin              = 10,
         extra_light       = 20,
         light             = 30,
         weight_normal     = 40,
         medium            = 50,
         semi_bold         = 60,
         bold              = 70,
         extra_bold        = 80,
         black             = 90,
         extra_black       = 95,
      };

      enum slant_enum
      {
         slant_normal      = 0,
         italic            = 90,
         oblique           = 100
      };

      enum stretch_enum
      {
         ultra_condensed	= 25,
         extra_condensed	= 31,
         condensed	      = 38,
         semi_condensed	   = 44,
         stretch_normal	   = 50,
         semi_expanded	   = 57,
         expanded	         = 63,
         extra_expanded	   = 75,
         ultra_expanded	   = 100
      };
   }

   struct font_descr
   {
      font_descr           normal() const;
      font_descr           size(float size_) const;

      font_descr           weight(font_constants::weight_enum w) const;
      font_descr           thin() const;
      font_descr           extra_light() const;
      font_descr           light() const;
      font_descr           weight_normal() const;
      font_descr           medium() const;
      font_descr           semi_bold() const;
      font_descr           bold() const;
      font_descr           extra_bold() const;
      font_descr           black() const;
      font_descr           extra_black() const;

      font_descr           style(font_constants::slant_enum s) const;
      font_descr           slant_normal() const;
      font_descr           italic() const;
      font_descr           oblique() const;

      font_descr           stretch(font_constants::stretch_enum s) const;
      font_descr           ultra_condensed() const;
      font_descr           extra_condensed() const;
      font_descr           condensed() const;
      font_descr           semi_condensed() const;
      font_descr           stretch_normal() const;
      font_descr           semi_expanded() const;
      font_descr           expanded() const;
      font_descr           extra_expanded() const;
      font_descr           ultra_expanded() const;

      using slant_enum = font_constants::slant_enum;

      std::string_view     _families;
      float                _size = 12;
      uint8_t              _weight = font_constants::weight_normal;
      slant_enum           _slant = font_constants::slant_normal;
      uint8_t              _stretch = font_constants::stretch_normal;
   };

   class font
   {
   public:
                           font();
                           font(font_descr descr);
                           font(font const& rhs);
                           font(font&& rhs) noexcept;
                           ~font();

      font&                operator=(font const& rhs);
      font&                operator=(font&& rhs) noexcept;
      explicit             operator bool() const;
      font_impl_ptr        impl() const;

      struct metrics_info
      {
         float             ascent;
         float             descent;
         float             leading;
      };

      metrics_info         metrics() const;
      float                line_height() const;
      float                measure_text(std::string_view str) const;

   private:

      friend class canvas;
      font_impl_ptr        _ptr;
   };

   ////////////////////////////////////////////////////////////////////////////
   // Inlines
   ////////////////////////////////////////////////////////////////////////////
   inline font_impl_ptr font::impl() const
   {
      return _ptr;
   }

   inline font_descr font_descr::normal() const
   {
      font_descr r = *this;
      r._weight = font_constants::weight_normal;
      r._slant = font_constants::slant_normal;
      r._stretch = font_constants::stretch_normal;
      return r;
   }

   inline font_descr font_descr::size(float size_) const
   {
      font_descr r = *this;
      r._size = size_;
      return r;
   }

   inline font_descr font_descr::weight(font_constants::weight_enum w) const
   {
      font_descr r = *this;
      r._weight = w;
      return r;
   }

   inline font_descr font_descr::thin() const
   {
      font_descr r = *this;
      r._weight = font_constants::thin;
      return r;
   }

   inline font_descr font_descr::extra_light() const
   {
      font_descr r = *this;
      r._weight = font_constants::extra_light;
      return r;
   }

   inline font_descr font_descr::light() const
   {
      font_descr r = *this;
      r._weight = font_constants::light;
      return r;
   }

   inline font_descr font_descr::weight_normal() const
   {
      font_descr r = *this;
      r._weight = font_constants::weight_normal;
      return r;
   }

   inline font_descr font_descr::medium() const
   {
      font_descr r = *this;
      r._weight = font_constants::medium;
      return r;
   }

   inline font_descr font_descr::semi_bold() const
   {
      font_descr r = *this;
      r._weight = font_constants::semi_bold;
      return r;
   }

   inline font_descr font_descr::bold() const
   {
      font_descr r = *this;
      r._weight = font_constants::bold;
      return r;
   }

   inline font_descr font_descr::extra_bold() const
   {
      font_descr r = *this;
      r._weight = font_constants::extra_bold;
      return r;
   }

   inline font_descr font_descr::black() const
   {
      font_descr r = *this;
      r._weight = font_constants::black;
      return r;
   }

   inline font_descr font_descr::extra_black() const
   {
      font_descr r = *this;
      r._weight = font_constants::extra_black;
      return r;
   }

   inline font_descr font_descr::style(font_constants::slant_enum s) const
   {
      font_descr r = *this;
      r._slant = s;
      return r;
   }

   inline font_descr font_descr::slant_normal() const
   {
      font_descr r = *this;
      r._slant = font_constants::slant_normal;
      return r;
   }

   inline font_descr font_descr::italic() const
   {
      font_descr r = *this;
      r._slant = font_constants::italic;
      return r;
   }

   inline font_descr font_descr::oblique() const
   {
      font_descr r = *this;
      r._slant = font_constants::oblique;
      return r;
   }

   inline font_descr font_descr::stretch(font_constants::stretch_enum s) const
   {
      font_descr r = *this;
      r._stretch = s;
      return r;
   }

   inline font_descr font_descr::ultra_condensed() const
   {
      font_descr r = *this;
      r._stretch = font_constants::ultra_condensed;
      return r;
   }

   inline font_descr font_descr::extra_condensed() const
   {
      font_descr r = *this;
      r._stretch = font_constants::extra_condensed;
      return r;
   }

   inline font_descr font_descr::condensed() const
   {
      font_descr r = *this;
      r._stretch = font_constants::condensed;
      return r;
   }

   inline font_descr font_descr::semi_condensed() const
   {
      font_descr r = *this;
      r._stretch = font_constants::semi_condensed;
      return r;
   }

   inline font_descr font_descr::stretch_normal() const
   {
      font_descr r = *this;
      r._stretch = font_constants::stretch_normal;
      return r;
   }

   inline font_descr font_descr::semi_expanded() const
   {
      font_descr r = *this;
      r._stretch = font_constants::semi_expanded;
      return r;
   }

   inline font_descr font_descr::expanded() const
   {
      font_descr r = *this;
      r._stretch = font_constants::expanded;
      return r;
   }

   inline font_descr font_descr::extra_expanded() const
   {
      font_descr r = *this;
      r._stretch = font_constants::extra_expanded;
      return r;
   }

   inline font_descr font_descr::ultra_expanded() const
   {
      font_descr r = *this;
      r._stretch = font_constants::ultra_expanded;
      return r;
   }

   inline font::operator bool() const
   {
      return bool(_ptr);
   }

   inline float font::line_height() const
   {
      auto m = metrics();
      return m.ascent + m.descent + m.leading;
   }

#ifdef __APPLE__
   fs::path get_user_fonts_directory();
#endif
}

#endif
