/*=============================================================================
   Copyright (c) 2021 Chance Snow, Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#ifndef __ARTIST_PATH_H
#define __ARTIST_PATH_H

#include "circle.h"
#include "rect.h"
#include "point.h"
#include "strings.h"

#ifdef __cplusplus
extern "C" {
#endif

   typedef struct path path;

   path*          artist_path_create();
   void           artist_path_destroy(path* path);

   path*          artist_path_create_from_rect(rect const& r);
   path*          artist_path_create_from_round_rect(rect const& r, float radius);
   path*          artist_path_create_from_circle(circle const& c);
   path*          artist_path_create_from_svg(string_view* svg_def);

   bool           artist_path_is_empty(path* path);
   bool           artist_path_includes_pt(path* path, point p);
   bool           artist_path_includes(path* path, float x, float y);
   rect           artist_path_bounds(path* path);

   void           artist_path_close(path* path);

   void           artist_path_add_rect(path* path, rect const& r);
   void           artist_path_add_round_rect(path* path, rect const& r, float radius);
   void           artist_path_add_circle(path* path, circle const& c);

   void           artist_path_add_rect_xywh(path* path, float x, float y, float width, float height);
   void           artist_path_add_round_rect_xywh(
                     path* path,
                     float x, float y,
                     float width, float height,
                     float radius
                  );
   void           artist_path_add_circle_xy(path* path, float cx, float cy, float radius);

   void           artist_path_move_to_pt(path* path, point p);
   void           artist_path_line_to_pt(path* path, point p);
   void           artist_path_arc_to_pt(path* path, point p1, point p2, float radius);
   void           artist_path_arc_pt(
                     path* path,
                     point p, float radius,
                     float start_angle, float end_angle,
                     bool ccw = false
                  );

   void           artist_path_quadratic_curve_to_pts(path* path, point cp, point end);
   void           artist_path_bezier_curve_to_pts(path* path, point cp1, point cp2, point end);

   void           artist_path_move_to(path* path, float x, float y);
   void           artist_path_line_to(path* path, float x, float y);
   void           artist_path_arc_to(
                     path* path,
                     float x1, float y1,
                     float x2, float y2,
                     float radius
                  );
   void           artist_path_arc(
                     path* path,
                     float x, float y, float radius,
                     float start_angle, float end_angle,
                     bool ccw = false
                  );

   void           artist_path_quadratic_curve_to(path* path, float cpx, float cpy, float x, float y);
   void           artist_path_bezier_curve_to(
                     path* path,
                     float cp1x, float cp1y,
                     float cp2x, float cp2y,
                     float x, float y
                  );

   enum fill_rule_enum
   {
      fill_winding,
      fill_odd_even
   };

   void           artist_path_fill_rule(path* path, fill_rule_enum rule);

#if defined(ARTIST_QUARTZ_2D)
   fill_rule_enum artist_path_get_fill_rule(path* path);
#endif

#ifdef __cplusplus
}
#endif
#endif
