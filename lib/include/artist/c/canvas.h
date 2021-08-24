/*=============================================================================
   Copyright (c) 2021 Chance Snow, Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#ifndef __ARTIST_CANVAS_H
#define __ARTIST_CANVAS_H

#include <artist/canvas.hpp>

#include "affine_transform.h"
#include "circle.h"
#include "color.h"
#include "font.h"
#include "image.h"
#include "path.h"
#include "point.h"
#include "rect.h"
#include "strings.h"

#ifdef __cplusplus
using namespace cycfi;
extern "C" {
#endif

   typedef struct artist::canvas canvas;
   typedef struct artist::canvas_impl canvas_impl;

   canvas*           artist_canvas_create(canvas_impl* context_);
   void              artist_canvas_destroy(canvas* cnv);

   void              artist_canvas_pre_scale(canvas* cnv, float sc);
   float             artist_canvas_get_pre_scale(canvas* cnv);

   ///////////////////////////////////////////////////////////////////////////////////
   // Transforms
   void              artist_canvas_translate_pt(canvas* cnv, point p);
   void              artist_canvas_rotate(canvas* cnv, float rad);
   void              artist_canvas_scale_pt(canvas* cnv, point p);
   void              artist_canvas_skew(canvas* cnv, double sx, double sy);
   point             artist_canvas_device_to_user_pt(canvas* cnv, point p);
   point             artist_canvas_user_to_device_pt(canvas* cnv, point p);

   void              artist_canvas_translate(canvas* cnv, float x, float y);
   void              artist_canvas_scale_square(canvas* cnv, float xy);
   void              artist_canvas_scale(canvas* cnv, float x, float y);
   point             artist_canvas_device_to_user(canvas* cnv, float x, float y);
   point             artist_canvas_user_to_device(canvas* cnv, float x, float y);

   affine_transform  artist_canvas_get_transform(canvas* cnv);
   void              artist_canvas_transform(canvas* cnv, affine_transform const& mat);
   void              artist_canvas_transform_spec(canvas* cnv, double a, double b, double c, double d, double tx, double ty);

   ///////////////////////////////////////////////////////////////////////////////////
   // Paths
   void              artist_canvas_begin_path(canvas* cnv);
   void              artist_canvas_close_path(canvas* cnv);
   void              artist_canvas_fill(canvas* cnv);
   void              artist_canvas_fill_preserve(canvas* cnv);
   void              artist_canvas_stroke(canvas* cnv);
   void              artist_canvas_stroke_preserve(canvas* cnv);

   void              artist_canvas_clip(canvas* cnv);
   void              artist_canvas_path_clip(canvas* cnv, path* p);
   rect              artist_canvas_get_clip_extent(canvas* cnv);
   bool              artist_canvas_point_in_path_pt(canvas* cnv, point p);
   bool              artist_canvas_point_in_path(canvas* cnv, float x, float y);
   rect              artist_canvas_get_fill_extent(canvas* cnv);

   void              artist_canvas_move_to_pt(canvas* cnv, point p);
   void              artist_canvas_line_to_pt(canvas* cnv, point p);
   void              artist_canvas_arc_to_pts(canvas* cnv, point p1, point p2, float radius);
   void              artist_canvas_arc_pt(
                        canvas* cnv,
                        point p, float radius,
                        float start_angle, float end_angle,
                        bool ccw = false
                     );
   void              artist_canvas_add_rect(canvas* cnv, const rect& r);
   void              artist_canvas_add_round_rect(canvas* cnv, const rect& r, float radius);
   void              artist_canvas_add_circle(canvas* cnv, circle const& c);
   void              artist_canvas_add_path(canvas* cnv, path* p);
   void              artist_canvas_clear_rect(canvas* cnv, rect const& r);

   void              artist_canvas_quadratic_curve_to_pts(canvas* cnv, point cp, point end);
   void              artist_canvas_bezier_curve_to_pts(canvas* cnv, point cp1, point cp2, point end);

   void              artist_canvas_move_to(canvas* cnv, float x, float y);
   void              artist_canvas_line_to(canvas* cnv, float x, float y);
   void              artist_canvas_arc_to(
                        canvas* cnv,
                        float x1, float y1,
                        float x2, float y2,
                        float radius
                     );
   void              artist_canvas_arc(
                        canvas* cnv,
                        float x, float y, float radius,
                        float start_angle, float end_angle,
                        bool ccw = false
                     );
   void              artist_canvas_add_rect_xywh(canvas* cnv, float x, float y, float width, float height);
   void              artist_canvas_add_round_rect_xy(
                        canvas* cnv,
                        float x, float y,
                        float width, float height,
                        float radius
                     );
   void              artist_canvas_add_circle_xy(canvas* cnv, float cx, float cy, float radius);
   void              artist_canvas_clear_rect_xywh(canvas* cnv, float x, float y, float width, float height);

   void              artist_canvas_quadratic_curve_to(canvas* cnv, float cpx, float cpy, float x, float y);
   void              artist_canvas_bezier_curve_to(
                        canvas* cnv,
                        float cp1x, float cp1y,
                        float cp2x, float cp2y,
                        float x, float y
                     );

   ///////////////////////////////////////////////////////////////////////////////////
   // Styles
   typedef artist::canvas::line_cap_enum line_cap_enum;
   typedef artist::canvas::join_enum join_enum;

   void              artist_canvas_fill_style(canvas* cnv, color c);
   void              artist_canvas_stroke_style(canvas* cnv, color c);
   void              artist_canvas_line_width(canvas* cnv, float w);
   void              artist_canvas_line_cap(canvas* cnv, line_cap_enum cap);
   void              artist_canvas_line_join(canvas* cnv, join_enum join);
   void              artist_canvas_miter_limit(canvas* cnv, float limit = 10);
   void              artist_canvas_shadow_style_offset(canvas* cnv, point offset, float blur, color c);
   void              artist_canvas_shadow_style_offset_xy(canvas* cnv, float offsetx, float offsety, float blur, color c);
   void              artist_canvas_shadow_style(canvas* cnv, float blur, color c);

   void              artist_canvas_stroke_color(canvas* cnv, color c);
   void              artist_canvas_fill_color(canvas* cnv, color c);

   typedef artist::canvas::composite_op_enum composite_op_enum;

   void              artist_canvas_global_composite_operation(canvas* cnv, composite_op_enum mode);
   void              artist_canvas_composite_op(canvas* cnv, composite_op_enum mode);

   ///////////////////////////////////////////////////////////////////////////////////
   // Gradients
   typedef artist::canvas::color_stop color_stop;

   // TODO: Interop with C arrays
   // struct gradient
   // {
   //    void  add_color_stop(canvas* cnv, color_stop cs);
   //    void  add_color_stop(canvas* cnv, float offset, struct color color_);
   //    std::vector<color_stop> color_space = {};
   // };

   // struct linear_gradient : gradient
   // {
   //    linear_gradient(float startx, float starty, float endx, float endy)
   //       : start{ startx, starty }
   //       , end{ endx, endy }
   //    {}

   //    linear_gradient(point start, point end)
   //       : start{ start }
   //       , end{ end }
   //    {}

   //    point start = {};
   //    point end = {};
   // };

   // struct radial_gradient : gradient
   // {
   //    radial_gradient(
   //       float c1x, float c1y, float c1r,
   //       float c2x, float c2y, float c2r
   //    )
   //       : c1{ c1x, c1y }
   //       , c1_radius{ c1r }
   //       , c2{ c2x, c2y }
   //       , c2_radius{ c2r }
   //    {}

   //    radial_gradient(
   //       point c1, float c1r,
   //       point c2, float c2r
   //    )
   //       : c1{ c1 }
   //       , c1_radius{ c1r }
   //       , c2{ c2 }
   //       , c2_radius{ c2r }
   //    {}

   //    point c1 = {};
   //    float c1_radius = {};
   //    point c2 = c1;
   //    float c2_radius = c1_radius;
   // };

   ///////////////////////////////////////////////////////////////////////////////////
   // More Styles
   // TODO: Interop with C arrays for gradients
   // void              artist_canvas_linear_gradient_fill_style(canvas* cnv, linear_gradient const& gr);
   // void              artist_canvas_radial_gradient_fill_style(canvas* cnv, radial_gradient const& gr);
   // void              artist_canvas_linear_gradient_stroke_style(canvas* cnv, linear_gradient const& gr);
   // void              artist_canvas_radial_gradient_stroke_style(canvas* cnv, radial_gradient const& gr);

   ///////////////////////////////////////////////////////////////////////////////////
   // Fill Rule

   void              artist_canvas_fill_rule(canvas* cnv, path::fill_rule_enum rule);

   ///////////////////////////////////////////////////////////////////////////////////
   // Rectangles
   void              artist_canvas_fill_rect(canvas* cnv, rect const& r);
   void              artist_canvas_fill_round_rect(canvas* cnv, rect const& r, float radius);
   void              artist_canvas_stroke_rect(canvas* cnv, rect const& r);
   void              artist_canvas_stroke_round_rect(canvas* cnv, rect const& r, float radius);

   void              artist_canvas_fill_rect_xywh(canvas* cnv, float x, float y, float width, float height);
   void              artist_canvas_fill_round_rect_xywh(canvas* cnv, float x, float y, float width, float height, float radius);
   void              artist_canvas_stroke_rect_xywh(canvas* cnv, float x, float y, float width, float height);
   void              artist_canvas_stroke_round_rect_xywh(canvas* cnv, float x, float y, float width, float height, float radius);

   ///////////////////////////////////////////////////////////////////////////////////
   // Font
   void              artist_canvas_font(canvas* cnv, font* const font_);

   ///////////////////////////////////////////////////////////////////////////////////
   // Text
   typedef artist::canvas::text_halign text_halign;
   typedef artist::canvas::text_valign text_valign;
   typedef artist::canvas::text_metrics text_metrics;

   void              artist_canvas_fill_text_pt(canvas* cnv, string_view* utf8, point p);
   void              artist_canvas_stroke_text_pt(canvas* cnv, string_view* utf8, point p);
   text_metrics      artist_canvas_measure_text(canvas* cnv, string_view* utf8);

   void              artist_canvas_text_align(canvas* cnv, int align);
   void              artist_canvas_text_halign(canvas* cnv, text_halign align);
   void              artist_canvas_text_baseline(canvas* cnv, text_valign align);

   void              artist_canvas_fill_text(canvas* cnv, string_view* utf8, float x, float y);
   void              artist_canvas_stroke_text(canvas* cnv, string_view* utf8, float x, float y);

   ///////////////////////////////////////////////////////////////////////////////////
   // Pixmaps

   void              artist_canvas_draw_pic_src_dst(canvas* cnv, image const& pic, rect const& src, rect const& dest);
   void              artist_canvas_draw_pic_dst(canvas* cnv, image const& pic, rect const& dest);
   void              artist_canvas_draw_pic_pos(canvas* cnv, image const& pic, point pos = {0, 0 });
   void              artist_canvas_draw_pic_pos_scale(canvas* cnv, image const& pic, point pos, float scale);
   void              artist_canvas_draw_pic_xy(canvas* cnv, image const& pic, float posx, float posy);
   void              artist_canvas_draw_pic_xy_scale(canvas* cnv, image const& pic, float posx, float posy, float scale);

#ifdef __cplusplus
}
#endif
#endif
