/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

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
      delete _impl;
   }

   path::path(path const& rhs)
    : _impl(new SkPath(*rhs._impl))
   {
   }

   path& path::operator=(path const& rhs)
   {
      if (this != &rhs)
         *_impl = *rhs._impl;
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

   rect path::bounds() const
   {
      auto const& r = _impl->getBounds();
      return { r.left(), r.top(), r.right(), r.bottom() };
   }

   void path::close()
   {
      _impl->close();
   }

   void path::add(rect r)
   {
      _impl->addRect({ r.left, r.top, r.right, r.bottom });
   }

   void path::add(rect r, float radius)
   {
      _impl->addRoundRect({ r.left, r.top, r.right, r.bottom }, radius, radius);
   }

   void path::add(struct circle c)
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
      if (!ccw)
         sweep = -sweep;

       _impl->addArc(
         { p.x-radius, p.y-radius, p.x+radius, p.y+radius },
         start, sweep
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
}
