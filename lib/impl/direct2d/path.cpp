/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <infra/support.hpp>
#include <artist/path.hpp>
#include <path_impl.hpp>
#include <vector>

namespace cycfi::artist
{
   path::path()
    : _impl{ new path_impl }
   {
   }

   path::~path()
   {
      delete _impl;
   }

   path::path(path const& rhs)
    : _impl{ new path_impl{ *rhs._impl } }
   {
   }

   path::path(path&& rhs)
    : _impl{ rhs._impl }
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
      return false;
   }

   bool path::is_empty() const
   {
      return false;
   }

   bool path::includes(point p) const
   {
      return false;
   }

   rect path::bounds() const
   {
      return {};
   }

   void path::begin_path()
   {
      _impl->begin_path();
   }

   void path::close()
   {
      _impl->close_path();
   }

   void path::add(rect r)
   {
      _impl->add(r);
   }

   void path::add(rect r, float radius)
   {
      _impl->add(r, radius);
   }

   void path::add(struct circle c)
   {
      _impl->add(c);
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
      if (start_angle == std::fmod(end_angle, 2 * pi))
         _impl->add(circle{ p, radius });
      else
        _impl->arc(p, radius, start_angle, end_angle, ccw);
   }

   void path::quadratic_curve_to(point cp, point end)
   {
   }

   void path::bezier_curve_to(point cp1, point cp2, point end)
   {
   }

   void path::fill_rule(path::fill_rule_enum rule)
   {
      _impl->fill_rule(
         rule == fill_winding?
         D2D1_FILL_MODE_WINDING : D2D1_FILL_MODE_ALTERNATE
      );
   }
}
