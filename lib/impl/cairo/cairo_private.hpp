/*=============================================================================
   Copyright (c) 2016-2024 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]

   Private Cairo backend types — NOT part of the public Artist API.
   Include only from Cairo backend implementation files.
=============================================================================*/
#pragma once
#include <cairo.h>
#include <artist/path.hpp>

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
