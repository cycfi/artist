/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]

   The public `path` class, delegating to d2d::path_impl. Ported from the 2020
   branch and adapted to develop's path API (add_rect/add_round_rect_impl,
   bounds/includes/is_empty, fill_rule).
=============================================================================*/
#include <artist/path.hpp>
#include <infra/support.hpp>   // pi
#include "path_impl.hpp"
#include <cmath>

namespace cycfi::artist
{
   path::path()
    : _impl{new d2d::path_impl}
   {
   }

   path::~path()
   {
      delete _impl;
   }

   path::path(path const& rhs)
    : _impl{new d2d::path_impl{*rhs._impl}}
   {
   }

   path::path(path&& rhs)
    : _impl{rhs._impl}
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
         delete _impl;
         _impl = rhs._impl;
         rhs._impl = nullptr;
      }
      return *this;
   }

   bool path::operator==(path const& /*rhs*/) const
   {
      return false;   // geometry equality is not meaningfully defined here
   }

   bool path::is_empty() const
   {
      return _impl->empty();
   }

   bool path::includes(point p) const
   {
      bool owned = false;
      auto geom = _impl->realize_fill(owned);
      if (!geom)
         return false;
      BOOL contains = FALSE;
      geom->FillContainsPoint(
         D2D1::Point2F(p.x, p.y), D2D1::Matrix3x2F::Identity(), &contains);
      if (owned)
         d2d::release(geom);
      return contains == TRUE;
   }

   rect path::bounds() const
   {
      return _impl->fill_bounds();
   }

   void path::close()
   {
      _impl->close_path();
   }

   void path::add_rect(rect const& r)
   {
      _impl->add(r);
   }

   void path::add_round_rect_impl(rect const& r, float radius)
   {
      _impl->add(r, radius);
   }

   void path::move_to(point p)
   {
      _impl->move_to(p);
   }

   void path::line_to(point p)
   {
      _impl->line_to(p);
   }

   void path::arc_to(point p1, point p2, float radius)
   {
      _impl->arc_to(p1, p2, radius);
   }

   void path::arc(
      point p, float radius,
      float start_angle, float end_angle,
      bool ccw)
   {
      // Full circle: use a native ellipse (a single arc segment can't span 2π
      // and a start==end arc is degenerate).
      if (start_angle == std::fmod(end_angle, 2 * float(pi)))
         _impl->add(circle{p.x, p.y, radius});
      else
         _impl->arc(p, radius, start_angle, end_angle, ccw);
   }

   void path::quadratic_curve_to(point cp, point end)
   {
      _impl->quadratic_curve_to(cp, end);
   }

   void path::bezier_curve_to(point cp1, point cp2, point end)
   {
      _impl->bezier_curve_to(cp1, cp2, end);
   }

   void path::fill_rule(fill_rule_enum rule)
   {
      _impl->fill_rule(
         rule == fill_winding?
         D2D1_FILL_MODE_WINDING : D2D1_FILL_MODE_ALTERNATE
      );
   }
}
