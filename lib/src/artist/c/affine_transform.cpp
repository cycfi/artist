/*=============================================================================
   Copyright (c) 2021 Chance Snow, Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <artist/affine_transform.hpp>
#include <artist/c/affine_transform.h>

using namespace cycfi;
extern "C" {
   bool                       artist_affine_transform_is_identity(affine_transform t) { return t.is_identity(); }
   affine_transform           artist_affine_transform_translate(affine_transform t, double tx, double ty) { return t.translate(tx, ty); }
   affine_transform           artist_affine_transform_scale(affine_transform t, double sx, double sy) { return t.scale(sx, sy); }
   affine_transform           artist_affine_transform_scale_square(affine_transform t, double sc) { return t.scale(sc); }
   inline affine_transform    artist_affine_transform_rotate(affine_transform t, double rad) { return t.rotate(rad); }
   inline affine_transform    artist_affine_transform_skew(affine_transform t, double sx, double sy) { return t.skew(sx, sy); }
   affine_transform           artist_affine_transform_invert(affine_transform t) { return t.invert(); }

   point                      artist_affine_transform_apply_pt(affine_transform t, point p) { return t.apply(p); }
   point                      artist_affine_transform_apply(affine_transform t, float x, float y) { return t.apply(x, y); }

   //                               template <std::size_t N>
   // void                       apply(point p[N]) const;
   void                       artist_affine_transform_apply_points(affine_transform t, point* p, std::size_t n) { return t.apply(p, n); }

   affine_transform           artist_affine_transform_make_translation(double tx, double ty) { return artist::make_translation(tx, ty); }
   affine_transform           artist_affine_transform_make_scale(double sx, double sy) { return artist::make_scale(sx, sy); }
   affine_transform           artist_affine_transform_make_scale_square(double sc) { return artist::make_scale(sc); }
   inline affine_transform    artist_affine_transform_make_rotation(double rad) { return artist::make_rotation(rad); }
   inline affine_transform    artist_affine_transform_make_skew(double sx, double sy) { return artist::make_skew(sx, sy); }
}
