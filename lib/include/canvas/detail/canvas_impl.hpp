/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#if !defined(ELEMENTS_DETAIL_CANVAS_IMPL_MAY_3_2016)
#define ELEMENTS_DETAIL_CANVAS_IMPL_MAY_3_2016

#ifndef M_PI
# define M_PI 3.14159265358979323846
#endif

namespace cycfi::elements
{
   ////////////////////////////////////////////////////////////////////////////
   // Inlines
   ////////////////////////////////////////////////////////////////////////////
   inline host_context* canvas::host_context() const
   {
      return _context;
   }

   inline void canvas::circle(struct circle c)
   {
      arc(point{ c.cx, c.cy }, c.radius, 0.0, 2 * M_PI);
   }

   inline void canvas::shadow_style(float blur, color c)
   {
      shadow_style({ 0.0f, 0.0f }, blur, c);
   }

   inline void canvas::linear_gradient::add_color_stop(color_stop cs)
   {
      space.push_back(cs);
   }

   inline void canvas::radial_gradient::add_color_stop(color_stop cs)
   {
      space.push_back(cs);
   }

   inline void canvas::fill_rect(struct rect r)
   {
      rect(r);
      fill();
   }

   inline void canvas::fill_round_rect(struct rect r, float radius)
   {
      round_rect(r, radius);
      fill();
   }

   inline void canvas::stroke_rect(struct rect r)
   {
      rect(r);
      stroke();
   }

   inline void canvas::stroke_round_rect(struct rect r, float radius)
   {
      round_rect(r, radius);
      stroke();
   }

   // inline void canvas::text_align(int align)
   // {
   //    _state.align = align;
   // }

   inline void canvas::draw(picture const& pic, struct rect dest)
   {
      draw(pic, {0, 0, pic.size() }, dest);
   }

   inline void canvas::draw(picture const& pic, point pos)
   {
      draw(pic, {0, 0, pic.size() }, {pos, pic.size() });
   }

   inline void canvas::draw(picture const& pic, point pos, float scale)
   {
      auto size = pic.size();
      auto scaled = extent{ size.x*scale, size.y*scale };
      draw(pic, {0, 0, size }, {pos, scaled });
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
}

#endif
