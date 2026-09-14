/*=============================================================================
   Copyright (c) 2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#if !defined(ARTIST_CANVAS_LAYER_SEPTEMBER_14_2026)
#define ARTIST_CANVAS_LAYER_SEPTEMBER_14_2026

#include <artist/canvas.hpp>

namespace cycfi::artist
{
   class canvas_layer_impl;

   ////////////////////////////////////////////////////////////////////////////
   // canvas_layer is an offscreen drawing surface made for one canvas: it
   // lives on that canvas's device (the GPU, when the canvas renders there)
   // at the canvas's current pixel density. It is meant for buffers redrawn
   // every frame, such as animations. Unlike offscreen_image it offers no
   // pixel access. Draw into it through context(), then onto the canvas
   // with canvas::draw. It is valid only for the canvas it was made for.
   ////////////////////////////////////////////////////////////////////////////
   class canvas_layer
   {
   public:

                        canvas_layer(canvas& cnv, extent size);
                        canvas_layer(canvas_layer&& rhs) noexcept;
                        ~canvas_layer();

                        canvas_layer(canvas_layer const&) = delete;
      canvas_layer&     operator=(canvas_layer const&) = delete;
      canvas_layer&     operator=(canvas_layer&& rhs) noexcept;

      canvas_impl*      context() const;
      extent            size() const;
      canvas_layer_impl* impl() const;

   private:

      canvas_layer_impl* _impl;
   };
}

#endif
