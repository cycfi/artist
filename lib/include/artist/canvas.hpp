/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#if !defined(ARTIST_CANVAS_MAY_3_2016)
#define ARTIST_CANVAS_MAY_3_2016

#include <artist/color.hpp>
#include <artist/rect.hpp>
#include <artist/circle.hpp>
#include <artist/image.hpp>
#include <artist/font.hpp>
#include <artist/path.hpp>
#include <artist/text_layout.hpp>

#include <vector>
#include <memory>

#if defined(ARTIST_SKIA)
class SkCanvas;
using canvas_impl = SkCanvas;
#endif

namespace cycfi::artist
{
#if defined(ARTIST_QUARTZ_2D)
   struct canvas_impl;
#endif

   using canvas_impl_ptr = canvas_impl*;

   class canvas
   {
   public:

      explicit          canvas(canvas_impl_ptr context_);
                        canvas(canvas const& rhs) = delete;
                        ~canvas();

                        canvas(canvas&& rhs) = delete;
      canvas&           operator=(canvas const& rhs) = delete;
      canvas_impl_ptr   impl() const;

      void              pre_scale(point p);

      ///////////////////////////////////////////////////////////////////////////////////
      // Transforms
      void              translate(point p);
      void              rotate(float rad);
      void              scale(point p);
      point             device_to_user(point p);
      point             user_to_device(point p);

      void              translate(float x, float y);
      void              scale(float x, float y);
      point             device_to_user(float x, float y);
      point             user_to_device(float x, float y);

      ///////////////////////////////////////////////////////////////////////////////////
      // Paths
      void              begin_path();
      void              close_path();
      void              fill();
      void              fill_preserve();
      void              stroke();
      void              stroke_preserve();

      void              clip();
      void              clip(path const& p);
      bool              hit_test(point p) const;

      void              move_to(point p);
      void              line_to(point p);
      void              arc_to(point p1, point p2, float radius);
      void              arc(
                           point p, float radius,
                           float start_angle, float end_angle,
                           bool ccw = false
                        );
      void              rect(struct rect r);
      void              round_rect(struct rect r, float radius);
      void              circle(struct circle c);
      void              path(class path const& p);

      void              quadratic_curve_to(point cp, point end);
      void              bezier_curve_to(point cp1, point cp2, point end);

      void              move_to(float x, float y);
      void              line_to(float x, float y);
      void              arc_to(
                           float x1, float y1,
                           float x2, float y2,
                           float radius
                        );
      void              arc(
                           float x, float y, float radius,
                           float start_angle, float end_angle,
                           bool ccw = false
                        );
      void              rect(float x, float y, float width, float height);
      void              round_rect(
                           float x, float y,
                           float width, float height,
                           float radius
                        );
      void              circle(float cx, float cy, float radius);

      void              quadratic_curve_to(float cpx, float cpy, float x, float y);
      void              bezier_curve_to(
                           float cp1x, float cp1y,
                           float cp2x, float cp2y,
                           float x, float y
                        );

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

      void              fill_style(color c);
      void              stroke_style(color c);
      void              line_width(float w);
      void              line_cap(line_cap_enum cap);
      void              line_join(join_enum join);
      void              miter_limit(float limit = 10);
      void              shadow_style(point offset, float blur, color c);
      void              shadow_style(float offsetx, float offsety, float blur, color c);
      void              shadow_style(float blur, color c);

      void              stroke_color(color c);
      void              fill_color(color c);

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

      void              global_composite_operation(composite_op_enum mode);
      void              composite_op(composite_op_enum mode);

      ///////////////////////////////////////////////////////////////////////////////////
      // Gradients
      struct color_stop
      {
         bool operator==(color_stop const& rhs) const;

         float          offset;
         struct color   color;
      };

      struct gradient
      {
         void  add_color_stop(color_stop cs);
         void  add_color_stop(float offset, struct color color_);
         std::vector<color_stop> color_space = {};

         bool operator==(gradient const& rhs) const;
         bool operator!=(gradient const& rhs) const   { return !(*this == rhs); }
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

         bool operator==(linear_gradient const& rhs) const;
         bool operator!=(linear_gradient const& rhs) const  { return !(*this == rhs); }

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

         bool operator==(radial_gradient const& rhs) const;
         bool operator!=(radial_gradient const& rhs) const  { return !(*this == rhs); }

         point c1 = {};
         float c1_radius = {};
         point c2 = c1;
         float c2_radius = c1_radius;
      };

      ///////////////////////////////////////////////////////////////////////////////////
      // More Styles
      void              fill_style(linear_gradient const& gr);
      void              fill_style(radial_gradient const& gr);
      void              stroke_style(linear_gradient const& gr);
      void              stroke_style(radial_gradient const& gr);

      ///////////////////////////////////////////////////////////////////////////////////
      // Fill Rule

      void              fill_rule(path::fill_rule_enum rule);

      ///////////////////////////////////////////////////////////////////////////////////
      // Rectangles
      void              fill_rect(struct rect r);
      void              fill_round_rect(struct rect r, float radius);
      void              stroke_rect(struct rect r);
      void              stroke_round_rect(struct rect r, float radius);

      void              fill_rect(float x, float y, float width, float height);
      void              fill_round_rect(float x, float y, float width, float height, float radius);
      void              stroke_rect(float x, float y, float width, float height);
      void              stroke_round_rect(float x, float y, float width, float height, float radius);

      ///////////////////////////////////////////////////////////////////////////////////
      // Font
      void              font(class font const& font_);

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

      void              fill_text(std::string_view utf8, point p);
      void              stroke_text(std::string_view utf8, point p);
      text_metrics      measure_text(std::string_view utf8);

      void              text_align(int align);
      void              text_align(text_halign align);
      void              text_baseline(text_valign align);

      void              fill_text(std::string_view utf8, float x, float y);
      void              stroke_text(std::string_view utf8, float x, float y);

      ///////////////////////////////////////////////////////////////////////////////////
      // Pixmaps

      void              draw(image const& pic, struct rect src, struct rect dest);
      void              draw(image const& pic, struct rect dest);
      void              draw(image const& pic, point pos = {0, 0 });
      void              draw(image const& pic, point pos, float scale);
      void              draw(image const& pic, float posx, float posy);
      void              draw(image const& pic, float posx, float posy, float scale);

      ///////////////////////////////////////////////////////////////////////////////////
      // States
      class state
      {
      public:
                        state(canvas& cnv_);
                        state(state&& rhs) noexcept;
                        state(state const&) = delete;
                        ~state();

         state&         operator=(state const&) = delete;
         state&         operator=(state&& rhs) noexcept;

      private:

         canvas* cnv;
      };

      state             new_state()   { return state{ *this }; }
      void              save();
      void              restore();

      class canvas_state;
      using canvas_state_ptr = std::unique_ptr<canvas_state>;

   private:

      canvas_impl_ptr   _context;
      canvas_state_ptr  _state;
   };
}

#include <artist/detail/canvas_inl.hpp>
#endif
