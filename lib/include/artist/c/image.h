/*=============================================================================
   Copyright (c) 2021 Chance Snow, Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#ifndef __ARTIST_IMAGE_H
#define __ARTIST_IMAGE_H

#include <artist/image.hpp>

#include "canvas.h"
#include "point.h"
#include "strings.h"

#ifdef __cplusplus
using namespace cycfi;
extern "C" {
#endif

   typedef artist::pixel_format pixel_format;

   ////////////////////////////////////////////////////////////////////////////
   // picture
   ////////////////////////////////////////////////////////////////////////////
   typedef struct artist::image image;

   image*            artist_image_create(float sizex, float sizey);
   image*            artist_image_create_with_size(extent size);
   image*            artist_image_create_from_file(const char* path_);
   image*            artist_image_create_from_data(uint8_t* data, pixel_format fmt, extent size);
   void              artist_image_destroy(image* img);

   extent            artist_image_size(image* img);
   void              artist_image_save_png(image* img, string_view* utf8);

   uint32_t*         artist_image_pixels(image* img);
   extent            artist_image_bitmap_size(image* img);

   ////////////////////////////////////////////////////////////////////////////
   // offscreen_image allows drawing into a picture
   ////////////////////////////////////////////////////////////////////////////
   typedef struct artist::offscreen_image offscreen_image;

   offscreen_image*  artist_offscreen_image_create(image* img);
   void              artist_offscreen_image_destroy(offscreen_image* img);

   typedef struct artist::canvas_impl canvas_impl;

   canvas_impl*      artist_offscreen_image_context(offscreen_image* img);

#ifdef __cplusplus
}
#endif
#endif
