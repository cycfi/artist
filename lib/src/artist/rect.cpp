/*=============================================================================
   Copyright (c) 2016-2023 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <infra/support.hpp>
#include <artist/rect.hpp>
#include <algorithm>

namespace cycfi::artist
{
   bool intersects(rect const& a, rect const& b)
   {
      if (a.left >= b.right || b.left >= a.right)
         return false;
      if (a.top >= b.bottom || b.top >= a.bottom)
         return false;
      return true;
   }

   rect union_(rect const& a, rect const& b)
   {
      return {
         std::min(a.left, b.left),
         std::min(a.top, b.top),
         std::max(a.right, b.right),
         std::max(a.bottom, b.bottom)
      };
   }

   rect intersection(rect const& a, rect const& b)
   {
      return {
         std::max(a.left, b.left),
         std::max(a.top, b.top),
         std::min(a.right, b.right),
         std::min(a.bottom, b.bottom)
      };
   }

   rect center(rect const& r_, rect const& encl)
   {
      rect r = r_;
      float dx = (encl.width() - r.width()) / 2.0;
      float dy = (encl.height() - r.height()) / 2.0;
      r = r.move_to(encl.left, encl.top);
      r = r.move(dx, dy);
      return r;
   }

   rect center_h(rect const& r_, rect const& encl)
   {
      rect r = r_;
      float dx = (encl.width() - r.width()) / 2.0;
      r = r.move_to(encl.left, r.top);
      r = r.move(dx, 0.0);
      return r;
   }

   rect center_v(rect const& r_, rect const& encl)
   {
      rect r = r_;
      float dy = (encl.height() - r.height()) / 2.0;
      r = r.move_to(r.left, encl.top);
      r = r.move(0.0, dy);
      return r;
   }

   rect align(rect const& r_, rect const& encl, float x_align, float y_align)
   {
      rect r = r_;
      r = r.move_to(
         encl.left + ((encl.width() - r.width()) * x_align),
         encl.top + ((encl.height() - r.height()) * y_align)
      );
      return r;
   }

   rect align_h(rect const& r_, rect const& encl, float x_align)
   {
      rect r = r_;
      r = r.move_to(
         encl.left + ((encl.width() - r.width()) * x_align), r.top
      );
      return r;
   }

   rect align_v(rect const& r_, rect const& encl, float y_align)
   {
      rect r = r_;
      r = r.move_to(
         r.left, encl.top + ((encl.height() - r.height()) * y_align)
      );
      return r;
   }
}
