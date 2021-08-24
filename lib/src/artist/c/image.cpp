/*=============================================================================
   Copyright (c) 2021 Chance Snow, Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <artist/image.hpp>

#include <artist/c/canvas.h>
#include <artist/c/image.h>
#include <artist/c/point.h>

using namespace cycfi;

extern "C" {
   ////////////////////////////////////////////////////////////////////////////
   // picture
   ////////////////////////////////////////////////////////////////////////////
   image*            artist_image_create(float sizex, float sizey) { return new artist::image(sizex, sizey); }
   image*            artist_image_create_with_size(extent size) { return new artist::image(size); }
   image*            artist_image_create_from_file(const char* path_) {
      fs::path path = path_;
      return new artist::image(path);
   }
   image*            artist_image_create_from_data(uint8_t* data, pixel_format fmt, extent size) { return new artist::image(data, fmt, size); }
   void              artist_image_destroy(image* img) { delete img; }

   extent            artist_image_size(image* img) { return img->size(); }
   void              artist_image_save_png(image* img, string_view* utf8) { img->save_png(*utf8); }

   uint32_t*         artist_image_pixels(image* img) { return img->pixels(); }
   extent            artist_image_bitmap_size(image* img) { return img->bitmap_size(); }

   ////////////////////////////////////////////////////////////////////////////
   // offscreen_image allows drawing into a picture
   ////////////////////////////////////////////////////////////////////////////
   offscreen_image*  artist_offscreen_image_create(image* img) { return new artist::offscreen_image(*img); }
   void              artist_offscreen_image_destroy(offscreen_image* img) { delete img; }

   canvas_impl*      artist_offscreen_image_context(offscreen_image* img) { img->context(); } 
}
