/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <artist/path.hpp>
#include <Quartz/Quartz.h>

namespace cycfi::artist
{
   path::path()
    : _impl(CGPathCreateMutable())
   {
   }

   path::~path()
   {
      if (_impl)
         CGPathRelease(_impl);
   }

   path::path(path const& rhs)
    : _impl(CGPathCreateMutableCopy(rhs._impl))
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
      {
         CGPathRelease(_impl);
         _impl = CGPathCreateMutableCopy(rhs.impl());
      }
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
      return CGPathEqualToPath(_impl, rhs._impl);
   }

   bool path::is_empty() const
   {
      return CGPathIsEmpty(_impl);
   }

   bool path::includes(point p) const
   {
      return CGPathContainsPoint(
         _impl, nullptr, { p.x, p.y }, _fill_rule == fill_odd_even
      );
   }

   rect path::bounds() const
   {
      auto r = CGPathGetBoundingBox(_impl);
      return rect(
         r.origin.x
       , r.origin.y
       , extent(r.size.width, r.size.height)
      );
   }

   void path::close()
   {
      CGPathCloseSubpath(_impl);
   }

   void path::add(rect r)
   {
      CGPathAddRect(_impl, nullptr,
         CGRect{ { r.left, r.top }, { r.width(), r.height() } }
      );
   }

   void path::add(rect r, float radius)
   {
      CGPathAddRoundedRect(_impl, nullptr,
         CGRect{ { r.left, r.top }, { r.width(), r.height() } }
       , radius, radius
      );
   }

   void path::move_to(point p)
   {
      CGPathMoveToPoint(_impl, nullptr, p.x, p.y);
   }

   void path::line_to(point p)
   {
      CGPathAddLineToPoint(_impl, nullptr, p.x, p.y);
   }

   void path::arc_to(point p1, point p2, float radius)
   {
      CGPathAddArcToPoint(
         _impl, nullptr
       , p1.x, p1.y, p2.x, p2.y, radius
      );
   }

   void path::arc(
      point p, float radius,
      float start_angle, float end_angle,
      bool ccw)
   {
      CGPathAddArc(
         _impl, nullptr
       , p.x, p.y, radius, start_angle, end_angle, !ccw
      );
   }

   void path::quadratic_curve_to(point cp, point end)
   {
      CGPathAddQuadCurveToPoint(_impl, nullptr, cp.x, cp.y, end.x, end.y);
   }

   void path::bezier_curve_to(point cp1, point cp2, point end)
   {
      CGPathAddCurveToPoint(_impl, nullptr, cp1.x, cp1.y, cp2.x, cp2.y, end.x, end.y);
   }
}
