/*=============================================================================
   Copyright (c) 2016-2023 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <infra/support.hpp>
#include <artist/path.hpp>
#include <SkPath.h>

namespace cycfi::artist
{
   path::path()
    : _impl(new SkPath)
   {
   }

   path::~path()
   {
      if (_impl)
         delete _impl;
   }

   path::path(path const& rhs)
    : _impl(new SkPath(*rhs._impl))
   {
   }

   path::path(path&& rhs)
    : _impl(rhs._impl)
   {
      rhs._impl = nullptr;
   }

   path& path::operator=(path const& rhs)
   {
      if (this != &rhs)
         *_impl = *rhs._impl;
      return *this;
   }

   path& path::operator=(path&& rhs)
   {
      if (this != &rhs)
      {
         _impl = rhs._impl;
         rhs._impl = nullptr;
      }
      return *this;
   }

   bool path::operator==(path const& rhs) const
   {
      return *_impl == *rhs._impl;
   }

   bool path::is_empty() const
   {
      return _impl->isEmpty();
   }

   bool path::includes(point p) const
   {
      return _impl->contains(p.x, p.y);
   }

// See comments at the bottom of path.hpp
#if !defined(_WIN32)
  rect path::bounds() const
  {
     auto const& r = _impl->getBounds();
     return rect{r.fLeft, r.fTop, r.fRight, r.fBottom};
  }
#endif

   void path::close()
   {
      _impl->close();
   }

   void path::add_rect(rect const& r)
   {
      _impl->addRect({r.left, r.top, r.right, r.bottom});
   }

   void path::add_circle(circle const& c)
   {
      _impl->addCircle(c.cx, c.cy, c.radius);
   }

   void path::move_to(point p)
   {
      _impl->moveTo(p.x, p.y);
   }

   void path::line_to(point p)
   {
      _impl->lineTo(p.x, p.y);
   }

   void path::arc_to(point p1, point p2, float radius)
   {
      _impl->arcTo(p1.x, p1.y, p2.x, p2.y, radius);
   }

   void path::arc(
      point p, float radius,
      float start_angle, float end_angle,
      bool ccw)
   {
      auto start = start_angle * 180 / pi;
      auto sweep = (end_angle - start_angle) * 180 / pi;
      if (sweep < 0)
         sweep += 360;
      sweep *= (ccw? -1 : 1);

      _impl->arcTo(
         {p.x-radius, p.y-radius, p.x+radius, p.y+radius},
         start, sweep, false
      );
   }

   void path::quadratic_curve_to(point cp, point end)
   {
      _impl->quadTo(cp.x, cp.y, end.x, end.y);
   }

   void path::bezier_curve_to(point cp1, point cp2, point end)
   {
      _impl->cubicTo(cp1.x, cp1.y, cp2.x, cp2.y, end.x, end.y);
   }

   void path::fill_rule(fill_rule_enum rule)
   {
      _impl->setFillType(rule == fill_winding? SkPathFillType::kWinding : SkPathFillType::kEvenOdd);
   }

   void path::add_round_rect_impl(rect const& r, float radius)
   {
      _impl->addRoundRect({r.left, r.top, r.right, r.bottom}, radius, radius);
   }

}
