/*=============================================================================
   Copyright (c) 2016-2024 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]

   Private Cairo backend types — NOT part of the public Artist API.
   Include only from Cairo backend implementation files.
=============================================================================*/
#pragma once
#include <cairo.h>
#include <artist/path.hpp>
#include <artist/font.hpp>

///////////////////////////////////////////////////////////////////////////////
// cairo_artist_path_t — backing storage for the Artist path class on Cairo.
// Uses a recording surface + cairo_t so that path operations can be
// accumulated and replayed later via cairo_append_path.

struct cairo_artist_path_t
{
   cairo_surface_t*                    surface   = nullptr;
   cairo_t*                            ctx       = nullptr;
   cycfi::artist::path::fill_rule_enum fill_rule =
      cycfi::artist::path::fill_winding;

   cairo_artist_path_t()
   {
      surface = cairo_recording_surface_create(CAIRO_CONTENT_COLOR_ALPHA, nullptr);
      ctx     = cairo_create(surface);
   }

   cairo_artist_path_t(cairo_artist_path_t const& rhs)
   {
      surface   = cairo_recording_surface_create(CAIRO_CONTENT_COLOR_ALPHA, nullptr);
      ctx       = cairo_create(surface);
      fill_rule = rhs.fill_rule;
      auto* cp  = cairo_copy_path(rhs.ctx);
      if (cp && cp->status == CAIRO_STATUS_SUCCESS)
         cairo_append_path(ctx, cp);
      cairo_path_destroy(cp);
   }

   ~cairo_artist_path_t()
   {
      if (ctx)     cairo_destroy(ctx);
      if (surface) cairo_surface_destroy(surface);
   }

   cairo_artist_path_t& operator=(cairo_artist_path_t const&) = delete;
};

///////////////////////////////////////////////////////////////////////////////
// font_impl — Cairo FreeType-backed scaled font with HarfBuzz shaping font.
// Declared as 'struct font_impl' inside namespace cycfi::artist in font.hpp.
// Each font_impl owns one Cairo reference to _scaled_font and one HarfBuzz
// font created via hb_ft_font_create_referenced.

#include <hb.h>
#include <infra/support.hpp>

namespace cycfi::artist
{
   struct font_impl
   {
      cairo_scaled_font_t* _scaled_font = nullptr;
      float                _size        = 0.0f;

      using hb_font_ptr = std::unique_ptr<hb_font_t, deleter<hb_font_t, hb_font_destroy>>;
      hb_font_ptr          _hb_font;

      font_impl() = default;

      font_impl(cairo_scaled_font_t* sf, float size, hb_font_ptr hb_fnt)
       : _scaled_font(sf)
       , _size(size)
       , _hb_font(std::move(hb_fnt))
      {}

      font_impl(font_impl const& rhs)
       : _scaled_font(rhs._scaled_font)
       , _size(rhs._size)
       , _hb_font(rhs._hb_font ? hb_font_ptr(hb_font_reference(rhs._hb_font.get())) : nullptr)
      {
         if (_scaled_font) cairo_scaled_font_reference(_scaled_font);
      }

      font_impl& operator=(font_impl const&) = delete;

      ~font_impl()
      {
         // _hb_font unique_ptr destructs first (hb_font_destroy), then:
         if (_scaled_font) cairo_scaled_font_destroy(_scaled_font);
      }
   };
}

///////////////////////////////////////////////////////////////////////////////
// image_impl — opaque Cairo image backing.
// Declared as 'class image_impl' inside namespace cycfi::artist in image.hpp.

namespace cycfi::artist
{
   class image_impl
   {
   public:
      cairo_surface_t* surface = nullptr;

      explicit image_impl(cairo_surface_t* s) : surface(s) {}

      ~image_impl()
      {
         if (surface)
            cairo_surface_destroy(surface);
      }

      image_impl(image_impl const&)            = delete;
      image_impl& operator=(image_impl const&) = delete;
   };
}
