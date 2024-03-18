/*=============================================================================
   Copyright (c) 2021 Chance Snow, Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <artist/path.hpp>

#include <artist/c/circle.h>
#include <artist/c/rect.h>
#include <artist/c/path.h>
#include <artist/c/point.h>
#include <artist/c/strings.h>

using namespace cycfi;

extern "C" {
   path*          artist_path_create();
   void           artist_path_destroy(path* path) { delete path; }

   path*          artist_path_create_from_rect(rect const& r) { return new artist::path(r); }
   path*          artist_path_create_from_round_rect(rect const& r, float radius) { return new artist::path(r, radius); }
   path*          artist_path_create_from_circle(circle const& c) { return new artist::path(c); }
   path*          artist_path_create_from_svg(string_view* svg_def) { return new artist::path(*svg_def); }

   bool           artist_path_is_empty(path* path) { return path->is_empty(); }
   bool           artist_path_includes_pt(path* path, point p) { return path->includes(p); }
   bool           artist_path_includes(path* path, float x, float y) { return path->includes(x, y); }
   rect           artist_path_bounds(path* path) { return path->bounds(); }

   void           artist_path_close(path* path) { path->close(); }

   void           artist_path_add_rect(path* path, rect const& r) { path->add_rect(r); }
   void           artist_path_add_round_rect(path* path, rect const& r, float radius) { path->add_round_rect(r, radius); }
   void           artist_path_add_circle(path* path, circle const& c) { path->add_circle(c); }

   void           artist_path_add_rect_xywh(path* path, float x, float y, float width, float height) { path->add_rect(x, y, width, height); }
   void           artist_path_add_round_rect_xywh(
                     path* path,
                     float x, float y,
                     float width, float height,
                     float radius
                  ) { path->add_round_rect(x, y, width, height, radius); }
   void           artist_path_add_circle_xy(path* path, float cx, float cy, float radius) { path->add_circle(cx, cy, radius); }

   void           artist_path_move_to_pt(path* path, point p) { path->move_to(p); }
   void           artist_path_line_to_pt(path* path, point p) { path->line_to(p); }
   void           artist_path_arc_to_pt(path* path, point p1, point p2, float radius) { path->arc_to(p1, p2, radius); }
   void           artist_path_arc_pt(
                     path* path,
                     point p, float radius,
                     float start_angle, float end_angle,
                     bool ccw
                  ) { path->arc(p, radius, start_angle, end_angle, ccw); }

   void           artist_path_quadratic_curve_to_pts(path* path, point cp, point end) { path->quadratic_curve_to(cp, end); }
   void           artist_path_bezier_curve_to_pts(path* path, point cp1, point cp2, point end) { path->bezier_curve_to(cp1, cp2, end); }

   void           artist_path_move_to(path* path, float x, float y) { path->move_to(x, y); }
   void           artist_path_line_to(path* path, float x, float y) { path->line_to(x, y); }
   void           artist_path_arc_to(
                     path* path,
                     float x1, float y1,
                     float x2, float y2,
                     float radius
                  ) { path->arc_to(x1, y1, x2, y2, radius); }
   void           artist_path_arc(
                     path* path,
                     float x, float y, float radius,
                     float start_angle, float end_angle,
                     bool ccw
                  ) { path->arc(x, y, radius, start_angle, end_angle, ccw); }

   void           artist_path_quadratic_curve_to(path* path, float cpx, float cpy, float x, float y) { path->quadratic_curve_to(cpx, cpy, x, y); }
   void           artist_path_bezier_curve_to(
                     path* path,
                     float cp1x, float cp1y,
                     float cp2x, float cp2y,
                     float x, float y
                  ) { path->bezier_curve_to(cp1x, cp1y, cp2x, cp2y, x, y); }

   void           artist_path_fill_rule(path* path, fill_rule_enum rule) { path->fill_rule(rule); }

#if defined(ARTIST_QUARTZ_2D)
   fill_rule_enum artist_path_get_fill_rule(path* path) { return path->fill_rule(); }
#endif

}
