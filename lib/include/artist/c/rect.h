#ifndef __ARTIST_RECT_H
#define __ARTIST_RECT_H

#include <artist/rect.hpp>

#include "point.h"

#ifdef __cplusplus
using namespace cycfi;
extern "C" {
#endif

   ////////////////////////////////////////////////////////////////////////////
   // rect
   ////////////////////////////////////////////////////////////////////////////
   typedef struct artist::rect rect;

   rect     artist_rect_create(point origin, float right, float bottom) {
      return rect(origin.x, origin.y, right, bottom);
   }
   rect     artist_rect_create(point top_left, point bottom_right) {
      return rect(top_left.x, top_left.y, bottom_right.x, bottom_right.y);
   }
   rect     artist_rect_create(float left, float top, extent size) {
      return rect(left, top, left + size.x, top + size.y);
   }
   rect     artist_rect_create(point origin, extent size) {
      return rect(origin.x, origin.y, origin.x + size.x, origin.y + size.y);
   }

   bool     artist_rect_is_empty(rect r) { return r.is_empty(); }
   bool     artist_rect_includes(rect r, point p) { return r.includes(); }
   bool     artist_rect_includes(rect r, rect const& other) { return r.includes(); }

   float    artist_rect_width(rect r) { return r.width(); }
   void     artist_rect_set_width(rect r, float width_) { r.width(width_); }
   float    artist_rect_height(rect r) { return r.height(); }
   void     artist_rect_set_height(rect r, float height_) { r.height(height_); }
   extent   artist_rect_size(rect r) { return r.size(); }
   void     artist_rect_set_size(rect r, extent size_) { r.size(size_); }

   point    artist_rect_top_left(rect r) { return r.top_left(); }
   point    artist_rect_bottom_right(rect r) { return r.bottom_right(); }
   point    artist_rect_top_right(rect r) { return r.top_right(); }
   point    artist_rect_bottom_left(rect r) { return r.bottom_left(); }

   rect     artist_rect_move(rect r, float dx, float dy) { return r.move(); }
   rect     artist_rect_move_to(rect r, float x, float y) { return r.move_to(); }
   rect     artist_rect_inset(rect r, float x_inset, float y_inset) { return r.inset(); }
   rect     artist_rect_inset(rect r, float xy_inset) { return r.inset(); }

   ////////////////////////////////////////////////////////////////////////////
   // Free Functions
   ////////////////////////////////////////////////////////////////////////////
   bool     artist_rect_is_valid(const rect r) { artist::is_valid(r); }
   bool     artist_rect_is_same_size(const rect a, const rect b) { artist::is_same_size(a, b); }
   bool     artist_rect_intersects(const rect a, const rect b) { artist::intersects(a, b); }

   point    artist_rect_center_point(const rect r) { artist::center_point(r); }
   float    artist_rect_area(const rect r) { artist::area(r); }
   rect     artist_rect_union_(const rect a, const rect b) { artist::union_(a, b); }
   rect     artist_rect_intersection(const rect a, const rect b) { artist::intersection(a, b); }

   void     artist_rect_clear(rect& r);
   rect     artist_rect_center(const rect r, const rect encl) { artist::center(r, encl); }
   rect     artist_rect_center_h(const rect r, const rect encl) { artist::center_h(r, encl); }
   rect     artist_rect_center_v(const rect r, const rect encl) { artist::center_v(r, encl); }
   rect     artist_rect_align(const rect r, const rect encl, float x_align, float y_align) { artist::align(r, encl, x_align, y_align); }
   rect     artist_rect_align_h(const rect r, const rect encl, float x_align) { artist::align_h(r, encl, x_align); }
   rect     artist_rect_align_v(const rect r, const rect encl, float y_align) { artist::align_v(r, encl, y_align); }

#ifdef __cplusplus
}
#endif
#endif
