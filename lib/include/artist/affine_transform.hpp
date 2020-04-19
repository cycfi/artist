/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#if !defined(ARTIST_AFFINE_TRANSFORM_APRIL_19_2020)
#define ARTIST_AFFINE_TRANSFORM_APRIL_19_2020

#include <cmath>

namespace cycfi::artist
{
    struct affine_transform
    {
        constexpr bool                  is_identity() const;
        constexpr affine_transform      translate(float tx, float ty);
        constexpr affine_transform      scale(float sx, float sy);
        inline affine_transform         rotate(float rad);
        constexpr affine_transform      invert();

        float a     = 1.0f;
        float b     = 0.0f;
        float c     = 0.0f;
        float d     = 1.0f;
        float tx    = 0.0f;
        float ty    = 0.0f;
    };

    constexpr affine_transform affine_identity;

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

    constexpr affine_transform concat(affine_transform t1, affine_transform t2)
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

    constexpr affine_transform make_translation(float tx, float ty)
    {
        return { 1.0f, 0.0f, 0.0f, 1.0f, tx, ty };
    }

    constexpr affine_transform make_scale(float sx, float sy)
    {
        return { sx, 0.0f, 0.0f, sy, 0.0f, 0.0f };
    }

    inline affine_transform make_rotation(float rad)
    {
        auto s = std::sinf(rad);
        auto c = std::cosf(rad);
        return { c, s, -s, c, 0.0f, 0.0f };
    }

    constexpr affine_transform affine_transform::translate(float tx, float ty)
    {
        return concat(*this, make_translation(tx, ty));
    }

    constexpr affine_transform affine_transform::scale(float sx, float sy)
    {
        return concat(*this, make_scale(sx, sy));
    }

    inline affine_transform affine_transform::rotate(float rad)
    {
        return concat(*this, make_rotation(rad));
    }

    constexpr affine_transform affine_transform::invert()
    {
	    float determinant = a * d - c * b;
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

