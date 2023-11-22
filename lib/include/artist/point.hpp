/*=============================================================================
   Copyright (c) 2016-2023 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#if !defined(ARTIST_POINT_APRIL_10_2016)
#define ARTIST_POINT_APRIL_10_2016

namespace cycfi::artist
{
   ////////////////////////////////////////////////////////////////////////////
   // Points
   ////////////////////////////////////////////////////////////////////////////
   struct point
   {
      constexpr         point();
      constexpr         point(float x, float y);
      constexpr         point(point const&) = default;
      constexpr point&  operator=(point const &) = default;

      constexpr bool    operator==(point const& other) const;
      constexpr bool    operator!=(point const& other) const;

      constexpr point   move(float dx, float dy) const;
      constexpr point   move_to(float x, float y) const;
      constexpr point   reflect(point p) const;

      float             x;
      float             y;
   };

   ////////////////////////////////////////////////////////////////////////////
   // Sizes
   ////////////////////////////////////////////////////////////////////////////
   struct extent : point
   {
      using point::point;

      constexpr         extent(point p)
                         : point(p)
                        {}

      point             move(float dx, float dy) const = delete;
      point             move_to(float x, float y) const = delete;
      point             reflect(point p) const = delete;
   };

   ////////////////////////////////////////////////////////////////////////////
   // Inlines
   ////////////////////////////////////////////////////////////////////////////
   inline constexpr point::point()
    : x(0.0), y(0.0)
   {}

   inline constexpr point::point(float x, float y)
    : x(x), y(y)
   {}

   inline constexpr bool point::operator==(point const& other) const
   {
      return (other.x == x) && (other.y == y);
   }

   inline constexpr bool point::operator!=(point const& other) const
   {
      return !(*this == other);
   }

   inline constexpr point point::move(float dx, float dy) const
   {
      point r = *this;
      r.x += dx;
      r.y += dy;
      return r;
   }

   inline constexpr point point::move_to(float x_, float y_) const
   {
      point r = *this;
      r.x = x_;
      r.y = y_;
      return r;
   }

   inline constexpr point point::reflect(point p) const
   {
      return {x + (x - p.x), y + (y - p.y)};
   }
}

#endif
