/*=============================================================================
   Copyright (c) 2021 Chance Snow, Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
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

   rect     artist_rect_create_with_origin(point origin, float right, float bottom);
   rect     artist_rect_create_with_lt_br(point top_left, point bottom_right);
   rect     artist_rect_create_with_lt_sz(float left, float top, extent size);
   rect     artist_rect_create_with_origin_sz(point origin, extent size);

   bool     artist_rect_is_empty(rect r);
   bool     artist_rect_includes_pt(rect r, point p);
   bool     artist_rect_includes(rect r, rect const& other);

   float    artist_rect_width(rect r);
   void     artist_rect_set_width(rect r, float width_);
   float    artist_rect_height(rect r);
   void     artist_rect_set_height(rect r, float height_);
   extent   artist_rect_size(rect r);
   void     artist_rect_set_size(rect r, extent size_);

   point    artist_rect_top_left(rect r);
   point    artist_rect_bottom_right(rect r);
   point    artist_rect_top_right(rect r);
   point    artist_rect_bottom_left(rect r);

   rect     artist_rect_move(rect r, float dx, float dy);
   rect     artist_rect_move_to(rect r, float x, float y);
   rect     artist_rect_inset(rect r, float x_inset, float y_inset);
   rect     artist_rect_inset_square(rect r, float xy_inset);

   ////////////////////////////////////////////////////////////////////////////
   // Free Functions
   ////////////////////////////////////////////////////////////////////////////
   bool     artist_rect_is_valid(const rect r);
   bool     artist_rect_is_same_size(const rect a, const rect b);
   bool     artist_rect_intersects(const rect a, const rect b);

   point    artist_rect_center_point(const rect r);
   float    artist_rect_area(const rect r);
   rect     artist_rect_union_(const rect a, const rect b);
   rect     artist_rect_intersection(const rect a, const rect b);

   void     artist_rect_clear(rect& r);
   rect     artist_rect_center(const rect r, const rect encl);
   rect     artist_rect_center_h(const rect r, const rect encl);
   rect     artist_rect_center_v(const rect r, const rect encl);
   rect     artist_rect_align(const rect r, const rect encl, float x_align, float y_align);
   rect     artist_rect_align_h(const rect r, const rect encl, float x_align);
   rect     artist_rect_align_v(const rect r, const rect encl, float y_align);

#ifdef __cplusplus
}
#endif
#endif
