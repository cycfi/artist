/*=============================================================================
   Copyright (c) 2016-2023 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#if !defined(ARTIST_DETAIL_CANVAS_IMPL_MAY_3_2016)
#define ARTIST_DETAIL_CANVAS_IMPL_MAY_3_2016

#include <algorithm>

namespace cycfi::artist
{
   ////////////////////////////////////////////////////////////////////////////
   // Inlines
   ////////////////////////////////////////////////////////////////////////////
   inline canvas_impl* canvas::impl() const
   {
      return _context;
   }

   inline canvas::operator bool() const
   {
      return _context != nullptr;
   }

   inline bool canvas::operator!() const
   {
      return _context == nullptr;
   }

   inline bool canvas::point_in_path(float x, float y) const
   {
      return point_in_path({x, y});
   }

   inline void canvas::translate(float x, float y)
   {
      translate({x, y});
   }

   inline void canvas::scale(float x, float y)
   {
      scale({x, y});
   }

   inline void canvas::scale(float xy)
   {
      scale({xy, xy});
   }

   inline point canvas::device_to_user(float x, float y)
   {
      return device_to_user({x, y});
   }

   inline point canvas::user_to_device(float x, float y)
   {
      return user_to_device({x, y});
   }

   inline void canvas::move_to(float x, float y)
   {
      move_to({x, y});
   }

   inline void canvas::line_to(float x, float y)
   {
      line_to({x, y});
   }

   inline void canvas::arc_to(
      float x1, float y1,
      float x2, float y2,
      float radius)
   {
      arc_to({x1, y1}, {x2, y2}, radius);
   }

   inline void canvas::arc(
      float x, float y, float radius,
      float start_angle, float end_angle,
      bool ccw)
   {
      arc({x, y}, radius, start_angle, end_angle, ccw);
   }

   inline void canvas::add_rect(float x, float y, float width, float height)
   {
      add_rect({x, y, extent{width, height}});
   }

   inline void canvas::add_round_rect(const rect& r, float radius)
   {
      radius = std::clamp(radius, 0.0f, std::min(r.width(), r.height()) / 2);
      add_round_rect_impl(r, radius);
   }

   inline void canvas::add_round_rect(
      float x, float y,
      float width, float height,
      float radius)
   {
      add_round_rect({x, y, extent{width, height}}, radius);
   }

   inline void canvas::add_circle(float cx, float cy, float radius)
   {
      add_circle({cx, cy, radius});
   }

   inline void canvas::clear_rect(float x, float y, float width, float height)
   {
      clear_rect({x, y, extent{width, height}});
   }

   inline void canvas::quadratic_curve_to(float cpx, float cpy, float x, float y)
   {
      quadratic_curve_to({cpx, cpy}, {x, y});
   }

   inline void canvas::bezier_curve_to(
      float cp1x, float cp1y,
      float cp2x, float cp2y,
      float x, float y)
   {
      bezier_curve_to({cp1x, cp1y}, {cp2x, cp2y}, {x, y});
   }

#if !defined(ARTIST_SKIA)
   inline void canvas::add_circle(struct circle const& c)
   {
      arc(point{c.cx, c.cy}, c.radius, 0.0, 2 * pi);
   }
#endif

   inline void canvas::stroke_color(color c)
   {
      stroke_style(c);
   }

   inline void canvas::fill_color(color c)
   {
      fill_style(c);
   }

   inline void canvas::shadow_style(float blur, color c)
   {
      shadow_style({0.0f, 0.0f}, blur, c);
   }

   inline void canvas::gradient::add_color_stop(color_stop cs)
   {
      color_space.push_back(cs);
   }

   inline void canvas::gradient::add_color_stop(float offset, struct color color_)
   {
      color_space.push_back({offset, color_});
   }

   inline void canvas::fill_rect(rect const& r)
   {
      add_rect(r);
      fill();
   }

   inline void canvas::fill_round_rect(rect const& r, float radius)
   {
      add_round_rect(r, radius);
      fill();
   }

   inline void canvas::stroke_rect(rect const& r)
   {
      add_rect(r);
      stroke();
   }

   inline void canvas::stroke_round_rect(rect const& r, float radius)
   {
      add_round_rect(r, radius);
      stroke();
   }

   inline void canvas::fill_rect(float x, float y, float width, float height)
   {
      fill_rect({x, y, extent{width, height}});
   }

   inline void canvas::fill_round_rect(float x, float y, float width, float height, float radius)
   {
      fill_round_rect({x, y, extent{width, height}}, radius);
   }

   inline void canvas::stroke_rect(float x, float y, float width, float height)
   {
      stroke_rect({x, y, extent{width, height}});
   }

   inline void canvas::stroke_round_rect(float x, float y, float width, float height, float radius)
   {
      stroke_round_rect({x, y, extent{width, height}}, radius);
   }

   inline void canvas::fill_text(std::string_view utf8, float x, float y)
   {
      fill_text(utf8, {x, y});
   }

   inline void canvas::stroke_text(std::string_view utf8, float x, float y)
   {
      stroke_text(utf8, {x, y});
   }

   inline void canvas::shadow_style(float offsetx, float offsety, float blur, color c)
   {
      shadow_style({offsetx, offsety}, blur, c);
   }

   inline void canvas::composite_op(composite_op_enum mode)
   {
      global_composite_operation(mode);
   }

   inline void canvas::draw(image const& pic, rect const& dest)
   {
      draw(pic, {0, 0, pic.size()}, dest);
   }

   inline void canvas::draw(image const& pic, point pos)
   {
      draw(pic, {0, 0, pic.size()}, {pos, pic.size()});
   }

   inline void canvas::draw(image const& pic, point pos, float scale)
   {
      auto size = pic.size();
      auto scaled = extent{size.x*scale, size.y*scale};
      draw(pic, {0, 0, size}, {pos, scaled});
   }

   inline void canvas::draw(image const& pic, float posx, float posy)
   {
      draw(pic, {posx, posy});
   }

   inline void canvas::draw(image const& pic, float posx, float posy, float scale)
   {
      draw(pic, {posx, posy}, scale);
   }

   inline canvas::state::state(canvas& cnv_)
     : cnv(&cnv_)
   {
      cnv->save();
   }

   inline canvas::state::state(state&& rhs) noexcept
    : cnv(rhs.cnv)
   {
      rhs.cnv = 0;
   }

   inline canvas::state::~state()
   {
      if (cnv)
         cnv->restore();
   }

   inline canvas::state& canvas::state::operator=(state&& rhs) noexcept
   {
      cnv = rhs.cnv;
      rhs.cnv = 0;
      return *this;
   }

   // Declared in context.hpp
   inline point device_to_user(point p, canvas& cnv)
   {
      return cnv.device_to_user(p);
   }

   // Declared in context.hpp
   inline rect device_to_user(rect const& r, canvas& cnv)
   {
      return {
         cnv.device_to_user(r.top_left())
       , cnv.device_to_user(r.bottom_right())
      };
   }
}

#endif
