/*=============================================================================
   Copyright (c) 2021 Chance Snow, Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#ifndef __ARTIST_AFFINE_TRANSFORM_H
#define __ARTIST_AFFINE_TRANSFORM_H

#include <stdbool.h>
#include <stddef.h>

#include "point.h"

#ifdef __cplusplus
extern "C" {
#endif

   typedef struct {
      double a;
      double b;
      double c;
      double d;
      double tx;
      double ty;
   } affine_transform;

   affine_transform           artist_affine_transform_identity();
   bool                       artist_affine_transform_is_identity(affine_transform t);
   affine_transform           artist_affine_transform_translate(affine_transform t, double tx, double ty);
   affine_transform           artist_affine_transform_scale(affine_transform t, double sx, double sy);
   affine_transform           artist_affine_transform_scale_square(affine_transform t, double sc);
   inline affine_transform    artist_affine_transform_rotate(affine_transform t, double rad);
   inline affine_transform    artist_affine_transform_skew(affine_transform t, double sx, double sy);
   affine_transform           artist_affine_transform_invert(affine_transform t);

   point                      artist_affine_transform_apply_pt(affine_transform t, point p);
   point                      artist_affine_transform_apply(affine_transform t, float x, float y);

   //                               template <std::size_t N>
   // void                       apply(point p[N]) const;
   void                       artist_affine_transform_apply_points(affine_transform t, point* p, size_t n);

   affine_transform           artist_affine_transform_make_translation(double tx, double ty);
   affine_transform           artist_affine_transform_make_scale(double sx, double sy);
   affine_transform           artist_affine_transform_make_scale_square(double sc);
   inline affine_transform    artist_affine_transform_make_rotation(double rad);
   inline affine_transform    artist_affine_transform_make_skew(double sx, double sy);

#ifdef __cplusplus
}
#endif
#endif
