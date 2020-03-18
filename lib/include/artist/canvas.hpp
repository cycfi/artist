/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#if !defined(ELEMENTS_CANVAS_MAY_3_2016)
#define ELEMENTS_CANVAS_MAY_3_2016

#include <artist/color.hpp>
#include <artist/rect.hpp>
#include <artist/circle.hpp>
#include <artist/picture.hpp>
#include <artist/font.hpp>
#include <artist/text_layout.hpp>

#include <vector>
#include <memory>

namespace cycfi::artist
{
   struct host_context;
   using host_context_ptr = host_context*;

   class canvas
   {
   public:

      explicit          canvas(host_context_ptr context_);
                        canvas(canvas const& rhs) = delete;
                        ~canvas();

                        canvas(canvas&& rhs) = delete;
      canvas&           operator=(canvas const& rhs) = delete;
      host_context_ptr  host_context() const;

      ///////////////////////////////////////////////////////////////////////////////////
      // Transforms
      void              translate(point p);
      void              rotate(float rad);
      void              scale(point p);
      point             device_to_user(point p);
      point             user_to_device(point p);

      ///////////////////////////////////////////////////////////////////////////////////
      // Paths
      void              begin_path();
      void              close_path();
      void              fill();
      void              fill_preserve();
      void              stroke();
      void              stroke_preserve();
      void              clip();
      bool              hit_test(point p) const;
      struct rect       fill_extent() const;

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

      void              quadratic_curve_to(point cp, point end);
      void              bezier_curve_to(point cp1, point cp2, point end);

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
      void              shadow_style(point p, float blur, color c);
      void              shadow_style(float blur, color c);

      ///////////////////////////////////////////////////////////////////////////////////
      // Gradients
      struct color_stop
      {
         float          offset;
         struct color   color;
      };

      struct linear_gradient
      {
         point start = {};
         point end = {};

         void  add_color_stop(color_stop cs);
         std::vector<color_stop> space = {};
      };

      struct radial_gradient
      {
         point c1 = {};
         float c1_radius = {};
         point c2 = c1;
         float c2_radius = c1_radius;

         void  add_color_stop(color_stop cs);
         std::vector<color_stop> space = {};
      };

      ///////////////////////////////////////////////////////////////////////////////////
      // More Styles
      void              fill_style(linear_gradient const& gr);
      void              fill_style(radial_gradient const& gr);
      void              stroke_style(linear_gradient const& gr);
      void              stroke_style(radial_gradient const& gr);

      ///////////////////////////////////////////////////////////////////////////////////
      // Fill Rule
      enum fill_rule_enum
      {
         fill_winding,
         fill_odd_even
      };

      void              fill_rule(fill_rule_enum rule);

      ///////////////////////////////////////////////////////////////////////////////////
      // Rectangles
      void              fill_rect(struct rect r);
      void              fill_round_rect(struct rect r, float radius);
      void              stroke_rect(struct rect r);
      void              stroke_round_rect(struct rect r, float radius);

      ///////////////////////////////////////////////////////////////////////////////////
      // Font
      void              font(class font const& font_);

      ///////////////////////////////////////////////////////////////////////////////////
      // Text
      enum text_alignment
      {
         // Horizontal align
         left     = 0,        // Default, align text horizontally to left.
         center   = 1,        // Align text horizontally to center.
         right    = 2,        // Align text horizontally to right.

         // Vertical align
         baseline = 4,        // Default, align text vertically to baseline.
         top      = 8,        // Align text vertically to top.
         middle   = 12,       // Align text vertically to middle.
         bottom   = 16        // Align text vertically to bottom.
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

      ///////////////////////////////////////////////////////////////////////////////////
      // Pixmaps

      void              draw(picture const& pic, struct rect src, struct rect dest);
      void              draw(picture const& pic, struct rect dest);
      void              draw(picture const& pic, point pos);
      void              draw(picture const& pic, point pos, float scale);

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

   private:

      struct canvas_state;
      using canvas_state_ptr = std::unique_ptr<canvas_state>;

      host_context_ptr  _context;
      canvas_state_ptr  _state;
   };
}

#include <artist/detail/canvas_impl.hpp>
#endif
