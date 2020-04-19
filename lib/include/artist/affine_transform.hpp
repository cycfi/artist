/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#if !defined(ARTIST_AFFINE_TRANSFORM_APRIL_19_2020)
#define ARTIST_AFFINE_TRANSFORM_APRIL_19_2020

#include <cmath>
#include <artist/point.hpp>

namespace cycfi::artist
{
   struct affine_transform
   {
      constexpr bool                  is_identity() const;
      constexpr affine_transform      translate(double tx, double ty) const;
      constexpr affine_transform      scale(double sx, double sy) const;
      constexpr affine_transform      scale(double sc) const;
      inline affine_transform         rotate(double rad) const;
      inline affine_transform         skew(double sx, double sy) const;
      constexpr affine_transform      invert() const;
      constexpr point                 apply(point p) const;
      constexpr point                 apply(float x, float y) const;

      double a    = 1.0;
      double b    = 0.0;
      double c    = 0.0;
      double d    = 1.0;
      double tx   = 0.0;
      double ty   = 0.0;
   };

   constexpr affine_transform affine_identity;

   constexpr affine_transform make_translation(double tx, double ty);
   constexpr affine_transform make_scale(double sx, double sy);
   constexpr affine_transform make_scale(double sc);
   inline affine_transform make_rotation(double rad);
   inline affine_transform make_skew(double sx, double sy);

   ///////////////////////////////////////////////////////////////////////////
   // Inlines and constexpr
   ///////////////////////////////////////////////////////////////////////////
   constexpr bool operator==(affine_transform const& lhs, affine_transform const& rhs)
   {
      return lhs.a == rhs.a && lhs.b == rhs.b && lhs.c == rhs.c
         && lhs.d == rhs.d && lhs.tx == rhs.tx && lhs.ty == rhs.ty
         ;
   }

   constexpr bool operator!=(affine_transform const& lhs, affine_transform const& rhs)
   {
      return !(rhs == lhs);
   }

   constexpr affine_transform operator*(affine_transform t1, affine_transform t2)
   {
      return {
         t2.a * t1.a + t2.b * t1.c
        , t2.a * t1.b + t2.b * t1.d
        , t2.c * t1.a + t2.d * t1.c
        , t2.c * t1.b + t2.d * t1.d
        , t2.tx * t1.a + t2.ty * t1.c + t1.tx
        , t2.tx * t1.b + t2.ty * t1.d + t1.ty
      };
   }

   constexpr bool affine_transform::is_identity() const
   {
      return *this == affine_identity;
   }

   constexpr point affine_transform::apply(point p) const
   {
      return {
         float(a * p.x + c * p.y + tx)
       , float(b * p.x + d * p.y + ty)
      };
   }

   constexpr point affine_transform::apply(float x, float y) const
   {
      return {
         float(a * x + c * y + tx)
       , float(b * x + d * y + ty)
      };
   }

   constexpr affine_transform make_translation(double tx, double ty)
   {
      return { 1.0, 0.0, 0.0, 1.0, tx, ty };
   }

   constexpr affine_transform make_scale(double sx, double sy)
   {
      return { sx, 0.0, 0.0, sy, 0.0, 0.0 };
   }

   constexpr affine_transform make_scale(double sc)
   {
      return { sc, 0.0, 0.0, sc, 0.0, 0.0 };
   }

   inline affine_transform make_rotation(double rad)
   {
      auto s = std::sin(rad);
      auto c = std::cos(rad);
      return { c, s, -s, c, 0.0, 0.0 };
   }

   inline affine_transform make_skew(double sx, double sy)
   {
      return { 1.0, std::tan(sx), std::tan(sy), 1.0, 0.0, 0.0 };
   }

   constexpr affine_transform affine_transform::translate(double tx, double ty) const
   {
      return *this * make_translation(tx, ty);
   }

   constexpr affine_transform affine_transform::scale(double sx, double sy) const
   {
      return *this * make_scale(sx, sy);
   }

   constexpr affine_transform affine_transform::scale(double sc) const
   {
      return *this * make_scale(sc);
   }

   inline affine_transform affine_transform::rotate(double rad) const
   {
      return *this * make_rotation(rad);
   }

   inline affine_transform affine_transform::skew(double sx, double sy) const
   {
      return *this * make_skew(sx, sy);
   }

   constexpr affine_transform affine_transform::invert() const
   {
      double determinant = a * d - c * b;
      if (determinant == 0)
         return *this;

      return {
         d  / determinant
       , -b  / determinant
       , -c  / determinant
       , a  / determinant
       , (-d * tx + c * ty) / determinant
       , (b * tx - a * ty) / determinant
      };
   }
}

#endif

