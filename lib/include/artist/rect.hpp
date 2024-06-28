/*=============================================================================
   Copyright (c) 2016-2023 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#if !defined(ARTIST_RECT_APRIL_10_2016)
#define ARTIST_RECT_APRIL_10_2016

#include <artist/point.hpp>

namespace cycfi::artist
{
   ////////////////////////////////////////////////////////////////////////////
   // rect
   ////////////////////////////////////////////////////////////////////////////
   struct rect
   {
      constexpr         rect();
      constexpr         rect(float left, float top, float right, float bottom);
      constexpr         rect(point origin, float right, float bottom)
                         : rect(origin.x, origin.y, right, bottom)
                        {}
      constexpr         rect(point top_left, point bottom_right)
                         : rect(top_left.x, top_left.y, bottom_right.x, bottom_right.y)
                        {}
      constexpr         rect(float left, float top, extent size)
                         : rect(left, top, left + size.x, top + size.y)
                        {}
      constexpr         rect(point origin, extent size)
                         : rect(origin.x, origin.y, origin.x + size.x, origin.y + size.y)
                        {}

      constexpr         rect(rect const&) = default;
      constexpr rect&   operator=(rect const&) = default;

      constexpr bool    operator==(rect const& other) const;
      constexpr bool    operator!=(rect const& other) const;

      constexpr bool    is_empty() const;
      constexpr bool    includes(point p) const;
      constexpr bool    includes(rect const& other) const;

      constexpr float   width() const;
      constexpr void    width(float width_);
      constexpr float   height() const;
      constexpr void    height(float height_);
      constexpr extent  size() const;
      constexpr void    size(extent size_);

      constexpr point   top_left() const;
      constexpr point   bottom_right() const;
      constexpr point   top_right() const;
      constexpr point   bottom_left() const;

      constexpr rect    move(float dx, float dy) const;
      constexpr rect    move_to(float x, float y) const;
      constexpr rect    inset(float x_inset, float y_inset) const;
      constexpr rect    inset(float xy_inset) const;

      float             left;
      float             top;
      float             right;
      float             bottom;
   };

   ////////////////////////////////////////////////////////////////////////////
   // Free Functions
   ////////////////////////////////////////////////////////////////////////////
   constexpr bool       is_valid(rect const& r);
   constexpr bool       is_same_size(rect const& a, rect const& b);
   bool                 intersects(rect const& a, rect const& b);

   constexpr point      center_point(rect const& r);
   constexpr float      area(rect const& r);
   rect                 union_(rect const& a, rect const& b);
   rect                 intersection(rect const& a, rect const& b);

   constexpr void       clear(rect& r);
   rect                 center(rect const& r, rect const& encl);
   rect                 center_h(rect const& r, rect const& encl);
   rect                 center_v(rect const& r, rect const& encl);
   rect                 align(rect const& r, rect const& encl, float x_align, float y_align);
   rect                 align_h(rect const& r, rect const& encl, float x_align);
   rect                 align_v(rect const& r, rect const& encl, float y_align);

   ////////////////////////////////////////////////////////////////////////////
   // Inlines
   ////////////////////////////////////////////////////////////////////////////
   constexpr rect::rect()
    : left(0.0), top(0.0), right(0.0), bottom(0.0)
   {}

   constexpr rect::rect(float left, float top, float right, float bottom)
    : left(left), top(top), right(right), bottom(bottom)
   {}

   constexpr bool rect::operator==(rect const& other) const
   {
      return
         (top == other.top) && (bottom == other.bottom) &&
         (left == other.left) && (right == other.right)
         ;
   }

   constexpr bool rect::operator!=(rect const& other) const
   {
      return !(*this == other);
   }

   constexpr bool rect::is_empty() const
   {
      return (left == right) || (top == bottom);
   }

   constexpr bool rect::includes(point p) const
   {
      return
         (p.x >= left) && (p.x <= right) &&
         (p.y >= top) && (p.y <= bottom)
         ;
   }

   constexpr bool rect::includes(rect const& other) const
   {
      return
         (other.left >= left) && (other.left <= right) &&
         (other.top >= top) && (other.top <= bottom) &&
         (other.right >= left) && (other.right <= right) &&
         (other.bottom >= top) && (other.bottom <= bottom)
         ;
   }

   constexpr float rect::width() const
   {
      return right - left;
   }

   constexpr void rect::width(float width_)
   {
      right = left + width_;
   }

   constexpr float rect::height() const
   {
      return (bottom - top);
   }

   constexpr void rect::height(float height_)
   {
      bottom = top + height_;
   }

   constexpr extent rect::size() const
   {
      return {width(), height()};
   }

   constexpr void rect::size(extent size_)
   {
      right = left + size_.x;
      bottom = top + size_.y;
   }

   constexpr point rect::top_left() const
   {
      return {left, top};
   }

   constexpr point rect::bottom_right() const
   {
      return {right, bottom};
   }

   constexpr point rect::top_right() const
   {
      return {right, top};
   }

   constexpr point rect::bottom_left() const
   {
      return {left, bottom};
   }

   constexpr rect rect::move(float dx, float dy) const
   {
      rect r = *this;
      r.top += dy;
      r.left += dx;
      r.bottom += dy;
      r.right += dx;
      return r;
   }

   constexpr rect rect::move_to(float x, float y) const
   {
      return move(x-left, y-top);
   }

   constexpr rect rect::inset(float x_inset, float y_inset) const
   {
      rect r = *this;
      r.top += y_inset;
      r.left += x_inset;
      r.bottom -= y_inset;
      r.right -= x_inset;

      if (!is_valid(r))
         clear(r);

      return r;
   }

   constexpr rect rect::inset(float xy_inset) const
   {
      return inset(xy_inset, xy_inset);
   }

   constexpr bool is_valid(rect const& r)
   {
      return (r.left <= r.right) && (r.top <= r.bottom);
   }

   constexpr bool is_same_size(rect const& a, rect const& b)
   {
      return (a.width() == b.width()) && (a.height() == b.height());
   }

   constexpr point center_point(rect const& r)
   {
      return {r.left + (r.width() / 2.0f), r.top + (r.height() / 2.0f)};
   }

   constexpr float area(rect const& r)
   {
      return r.width() * r.height();
   }

   constexpr void clear(rect& r)
   {
      r.left = 0.0;
      r.top  = 0.0;
      r.right = 0.0;
      r.bottom = 0.0;
   }

   inline constexpr float axis_extent(rect const &r, axis a)
   {
     return a == axis::x ? r.width() : r.height();
   }

   inline constexpr float axis_min(rect const &r, axis a)
   {
     return a == axis::x ? r.left : r.top;
   }

   inline constexpr float axis_max(rect const &r, axis a)
   {
     return a == axis::x ? r.right : r.bottom;
   }

   inline constexpr float& axis_min(rect &r, axis a)
   {
     return a == axis::x ? r.left : r.top;
   }

   inline constexpr float& axis_max(rect &r, axis a)
   {
     return a == axis::x ? r.right : r.bottom;
   }

   inline constexpr auto make_rect(axis a, float this_axis_min, float other_axis_min, float this_axis_max, float other_axis_max) -> rect
   {
     if (a == axis::x) {
       return rect(this_axis_min, other_axis_min, this_axis_max, other_axis_max);
     }
     else {
       return rect(other_axis_min, this_axis_min, other_axis_max, this_axis_max);
     }
   }
}

#endif
