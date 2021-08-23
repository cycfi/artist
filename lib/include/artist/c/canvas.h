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

#ifdef __cplusplus
using namespace cycfi;
extern "C" {
#endif

   typedef struct artist::canvas canvas;
   typedef struct artist::canvas_impl canvas_impl;

   canvas* artist_canvas_create(canvas_impl* context_) {
      return new artist::canvas(context_);
   }

   void artist_canvas_destroy(canvas* cnv) {
      delete cnv;
   }

   void              artist_canvas_pre_scale(canvas* cnv, float sc) { cnv->pre_scale(sc); }
   float             artist_canvas_get_pre_scale(canvas* cnv) { return cnv->pre_scale(); }

   ///////////////////////////////////////////////////////////////////////////////////
   // Transforms
   void              artist_canvas_translate(canvas* cnv, point p) { cnv->translate(p); }
   void              artist_canvas_rotate(canvas* cnv, float rad) { cnv->rotate(rad); }
   void              artist_canvas_scale(canvas* cnv, point p) { cnv->scale(p); }
   void              artist_canvas_skew(canvas* cnv, double sx, double sy) { cnv->skew(sx, sy); }
   point             artist_canvas_device_to_user(canvas* cnv, point p) { return cnv->device_to_user(p); }
   point             artist_canvas_user_to_device(canvas* cnv, point p) { return cnv->user_to_device(p); }

   void              artist_canvas_translate(canvas* cnv, float x, float y) { cnv->translate(x, y); }
   void              artist_canvas_scale(canvas* cnv, float xy) { cnv->scale(xy); }
   void              artist_canvas_scale(canvas* cnv, float x, float y) { cnv->scale(x, y); }
   point             artist_canvas_device_to_user(canvas* cnv, float x, float y) { return cnv->device_to_user(x, y); }
   point             artist_canvas_user_to_device(canvas* cnv, float x, float y) { return cnv->user_to_device(x, y); }

   affine_transform  artist_canvas_get_transform(canvas* cnv) { return cnv->transform(); }
   void              artist_canvas_transform(canvas* cnv, affine_transform const& mat) { cnv->transform(mat); }
   void              artist_canvas_transform(canvas* cnv, double a, double b, double c, double d, double tx, double ty) { cnv->transform(a, b, c, d, tx, ty); }

   ///////////////////////////////////////////////////////////////////////////////////
   // Paths
   void              artist_canvas_begin_path(canvas* cnv) { cnv->begin_path(); }
   void              artist_canvas_close_path(canvas* cnv) { cnv->close_path(); }
   void              artist_canvas_fill(canvas* cnv) { cnv->fill(); }
   void              artist_canvas_fill_preserve(canvas* cnv) { cnv->fill_preserve(); }
   void              artist_canvas_stroke(canvas* cnv) { cnv->stroke(); }
   void              artist_canvas_stroke_preserve(canvas* cnv) { cnv->stroke_preserve(); }

   void              artist_canvas_clip(canvas* cnv) { cnv->clip(); }
   void              artist_canvas_path_clip(canvas* cnv, path const& p) { cnv->clip(p); }
   rect              artist_canvas_get_clip_extent(canvas* cnv) { return cnv->clip_extent(); }
   bool              artist_canvas_point_in_path(canvas* cnv, point p) { return cnv->point_in_path(p); }
   bool              artist_canvas_point_in_path(canvas* cnv, float x, float y) { return cnv->point_in_path(x, y); }
   rect              artist_canvas_get_fill_extent(canvas* cnv) { return cnv->fill_extent(); }

   void              artist_canvas_move_to(canvas* cnv, point p) { cnv->move_to(p); }
   void              artist_canvas_line_to(canvas* cnv, point p) { cnv->line_to(p); }
   void              artist_canvas_arc_to(canvas* cnv, point p1, point p2, float radius) { cnv->arc_to(p1, p2, radius); }
   void              artist_canvas_arc(
                        canvas* cnv,
                        point p, float radius,
                        float start_angle, float end_angle,
                        bool ccw = false
                     ) { cnv->arc(p, radius, start_angle, end_angle, ccw); }
   void              artist_canvas_add_rect(canvas* cnv, const rect& r) { cnv->add_rect(r); }
   void              artist_canvas_add_round_rect(canvas* cnv, const rect& r, float radius) { cnv->add_round_rect(r, radius); }
   void              artist_canvas_add_circle(canvas* cnv, circle const& c) { cnv->add_circle(c); }
   void              artist_canvas_add_path(canvas* cnv, path const& p) { cnv->add_path(p); }
   void              artist_canvas_clear_rect(canvas* cnv, rect const& r) { cnv->clear_rect(r); }

   void              artist_canvas_quadratic_curve_to(canvas* cnv, point cp, point end) { cnv->quadratic_curve_to(cp, end); }
   void              artist_canvas_bezier_curve_to(canvas* cnv, point cp1, point cp2, point end) { cnv->bezier_curve_to(cp1, cp2, end); }

   void              artist_canvas_move_to(canvas* cnv, float x, float y) { cnv->move_to(x, y); }
   void              artist_canvas_line_to(canvas* cnv, float x, float y) { cnv->line_to(x, y); }
   void              artist_canvas_arc_to(
                        canvas* cnv,
                        float x1, float y1,
                        float x2, float y2,
                        float radius
                     ) { cnv->arc_to(x1, y1, x2, y2, radius); }
   void              artist_canvas_arc(
                        canvas* cnv,
                        float x, float y, float radius,
                        float start_angle, float end_angle,
                        bool ccw = false
                     ) { cnv->arc(x, y, radius, start_angle, end_angle, ccw); }
   void              artist_canvas_add_rect(canvas* cnv, float x, float y, float width, float height) { cnv->add_rect(x, y, width, height); }
   void              artist_canvas_add_round_rect(
                        canvas* cnv,
                        float x, float y,
                        float width, float height,
                        float radius
                     ) { cnv->add_round_rect(x, y, width, height, radius); }
   void              artist_canvas_add_circle(canvas* cnv, float cx, float cy, float radius) { cnv->add_circle(cx, cy, radius); }
   void              artist_canvas_clear_rect(canvas* cnv, float x, float y, float width, float height) { cnv->clear_rect(x, y, width, height); }

   void              artist_canvas_quadratic_curve_to(canvas* cnv, float cpx, float cpy, float x, float y) { cnv->quadratic_curve_to(cpx, cpy, x, y); }
   void              artist_canvas_bezier_curve_to(
                        canvas* cnv,
                        float cp1x, float cp1y,
                        float cp2x, float cp2y,
                        float x, float y
                     ) { cnv->bezier_curve_to(cp1x, cp1y, cp2x, cp2y, x, y); }

   ///////////////////////////////////////////////////////////////////////////////////
   // Styles

   enum line_cap_enum
   {
      butt,
      round,
      square
   };

   enum join_enum
   {
      bevel_join,
      round_join,
      miter_join
   };

   void              artist_canvas_fill_style(canvas* cnv, color c) { cnv->fill_style(c); }
   void              artist_canvas_stroke_style(canvas* cnv, color c) { cnv->stroke_style(c); }
   void              artist_canvas_line_width(canvas* cnv, float w) { cnv->line_width(w); }
   void              artist_canvas_line_cap(canvas* cnv, line_cap_enum cap) { cnv->line_cap(cap); }
   void              artist_canvas_line_join(canvas* cnv, join_enum join) { cnv->line_join(join); }
   void              artist_canvas_miter_limit(canvas* cnv, float limit = 10) { cnv->miter_limit(limit); }
   void              artist_canvas_shadow_style(canvas* cnv, point offset, float blur, color c) { cnv->shadow_style(offset, blur, c); }
   void              artist_canvas_shadow_style(canvas* cnv, float offsetx, float offsety, float blur, color c) { cnv->shadow_style(offsetx, offsety, blur, c); }
   void              artist_canvas_shadow_style(canvas* cnv, float blur, color c) { cnv->shadow_style(blur, c); }

   void              artist_canvas_stroke_color(canvas* cnv, color c) { cnv->stroke_color(c); }
   void              artist_canvas_fill_color(canvas* cnv, color c) { cnv->fill_color(c); }

   enum composite_op_enum
   {
      source_over,
      source_atop,
      source_in,
      source_out,

      destination_over,
      destination_atop,
      destination_in,
      destination_out,

      lighter,
      darker,
      copy,
      xor_,

      difference,
      exclusion,
      multiply,
      screen,

      color_dodge,
      color_burn,
      soft_light,
      hard_light,

      hue,
      saturation,
      color_op,
      luminosity
   };

   void              artist_canvas_global_composite_operation(canvas* cnv, composite_op_enum mode) { cnv->global_composite_operation(mode); }
   void              artist_canvas_composite_op(canvas* cnv, composite_op_enum mode) { cnv->composite_op(mode); }

   ///////////////////////////////////////////////////////////////////////////////////
   // Gradients
   struct color_stop
   {
      float          offset;
      struct color   color;
   };

   struct gradient
   {
      void  add_color_stop(canvas* cnv, color_stop cs);
      void  add_color_stop(canvas* cnv, float offset, struct color color_);
      std::vector<color_stop> color_space = {};
   };

   struct linear_gradient : gradient
   {
      linear_gradient(float startx, float starty, float endx, float endy)
         : start{ startx, starty }
         , end{ endx, endy }
      {}

      linear_gradient(point start, point end)
         : start{ start }
         , end{ end }
      {}

      point start = {};
      point end = {};
   };

   struct radial_gradient : gradient
   {
      radial_gradient(
         float c1x, float c1y, float c1r,
         float c2x, float c2y, float c2r
      )
         : c1{ c1x, c1y }
         , c1_radius{ c1r }
         , c2{ c2x, c2y }
         , c2_radius{ c2r }
      {}

      radial_gradient(
         point c1, float c1r,
         point c2, float c2r
      )
         : c1{ c1 }
         , c1_radius{ c1r }
         , c2{ c2 }
         , c2_radius{ c2r }
      {}

      point c1 = {};
      float c1_radius = {};
      point c2 = c1;
      float c2_radius = c1_radius;
   };

   ///////////////////////////////////////////////////////////////////////////////////
   // More Styles
   void              artist_canvas_linear_gradient_fill_style(canvas* cnv, linear_gradient const& gr) { cnv->fill_style(gr); }
   void              artist_canvas_radial_gradient_fill_style(canvas* cnv, radial_gradient const& gr) { cnv->fill_style(gr); }
   void              artist_canvas_linear_gradient_stroke_style(canvas* cnv, linear_gradient const& gr) { cnv->stroke_style(gr); }
   void              artist_canvas_radial_gradient_stroke_style(canvas* cnv, radial_gradient const& gr) { cnv->stroke_style(gr); }

   ///////////////////////////////////////////////////////////////////////////////////
   // Fill Rule

   void              artist_canvas_fill_rule(canvas* cnv, path::fill_rule_enum rule) { cnv->fill_rule(rule); }

   ///////////////////////////////////////////////////////////////////////////////////
   // Rectangles
   void              artist_canvas_fill_rect(canvas* cnv, rect const& r) { cnv->fill_rect(r); }
   void              artist_canvas_fill_round_rect(canvas* cnv, rect const& r, float radius) { cnv->fill_round_rect(r, radius); }
   void              artist_canvas_stroke_rect(canvas* cnv, rect const& r) { cnv->stroke_rect(r); }
   void              artist_canvas_stroke_round_rect(canvas* cnv, rect const& r, float radius) { cnv->stroke_round_rect(r, radius); }

   void              artist_canvas_fill_rect(canvas* cnv, float x, float y, float width, float height) { cnv->fill_rect(x, y, width, height); }
   void              artist_canvas_fill_round_rect(canvas* cnv, float x, float y, float width, float height, float radius) { cnv->fill_round_rect(x, y, width, height, radius); }
   void              artist_canvas_stroke_rect(canvas* cnv, float x, float y, float width, float height) { cnv->stroke_rect(x, y, width, height); }
   void              artist_canvas_stroke_round_rect(canvas* cnv, float x, float y, float width, float height, float radius) { cnv->stroke_round_rect(x, y, width, height, radius); }

   ///////////////////////////////////////////////////////////////////////////////////
   // Font
   void              artist_canvas_font(canvas* cnv, class font const& font_) { cnv->font(font_); }

   ///////////////////////////////////////////////////////////////////////////////////
   // Text
   enum text_halign     // Horizontal align
   {
      left,             // Default, align text horizontally to left.
      center,           // Align text horizontally to center.
      right             // Align text horizontally to right.
   };

   enum text_valign     // Vertical align
   {
      baseline = 4,     // Default, align text vertically to baseline.
      top      = 8,     // Align text vertically to top.
      middle   = 12,    // Align text vertically to middle.
      bottom   = 16     // Align text vertically to bottom.
   };

   struct text_metrics
   {
      float       ascent;
      float       descent;
      float       leading;
      point       size;
   };

   // typedef struct  string_view;
   std::string_view*  artist_string_view_from_utf8(const char* str) {
      std::string_view* result = (std::string_view*) malloc(sizeof(std::strlen));
      *result = std::string_view(str);
      return result;
   }
   void              artist_string_view_destroy(std::string_view* str) { free(str); }

   void              artist_canvas_fill_text(canvas* cnv, std::string_view* utf8, point p) { cnv->fill_text(*utf8, p); }
   void              artist_canvas_stroke_text(canvas* cnv, std::string_view* utf8, point p) { cnv->stroke_text(*utf8, p); }
   text_metrics      artist_canvas_measure_text(canvas* cnv, std::string_view* utf8) { cnv->measure_text(*utf8); }

   void              artist_canvas_text_align(canvas* cnv, int align) { cnv->text_align(align); }
   void              artist_canvas_text_align(canvas* cnv, text_halign align) { cnv->text_align(align); }
   void              artist_canvas_text_baseline(canvas* cnv, text_valign align) { cnv->text_baseline(align); }

   void              artist_canvas_fill_text(canvas* cnv, std::string_view* utf8, float x, float y) { cnv->fill_text(*utf8, x, y); }
   void              artist_canvas_stroke_text(canvas* cnv, std::string_view* utf8, float x, float y) { cnv->stroke_text(*utf8, x, y); }

   ///////////////////////////////////////////////////////////////////////////////////
   // Pixmaps

   void              artist_canvas_draw(canvas* cnv, image const& pic, rect const& src, rect const& dest) { cnv->draw(pic, src, dest); }
   void              artist_canvas_draw(canvas* cnv, image const& pic, rect const& dest) { cnv->draw(pic, dest); }
   void              artist_canvas_draw(canvas* cnv, image const& pic, point pos = {0, 0 }) { cnv->draw(pic, pos); }
   void              artist_canvas_draw(canvas* cnv, image const& pic, point pos, float scale) { cnv->draw(pic, pos, scale); }
   void              artist_canvas_draw(canvas* cnv, image const& pic, float posx, float posy) { cnv->draw(pic, posx, posy); }
   void              artist_canvas_draw(canvas* cnv, image const& pic, float posx, float posy, float scale) { cnv->draw(pic, posx, posy, scale); }

#ifdef __cplusplus
}
#endif
#endif
