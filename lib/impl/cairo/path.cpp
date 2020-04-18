/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <infra/support.hpp>
#include <artist/path.hpp>

namespace cycfi::artist
{
   path::path()
   {
   }

   path::~path()
   {
   }

   path::path(path const& rhs)
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
         ;
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
     return rect{};
   }

   void path::close()
   {
   }

   void path::add(rect r)
   {
   }

   void path::add(rect r, float radius)
   {
   }

   void path::add(struct circle c)
   {
   }

   void path::move_to(point p)
   {
   }

   void path::line_to(point p)
   {
   }

   void path::arc_to(point p1, point p2, float radius)
   {
   }

   void path::arc(
      point p, float radius,
      float start_angle, float end_angle,
      bool ccw)
   {
   }

   void path::quadratic_curve_to(point cp, point end)
   {
   }

   void path::bezier_curve_to(point cp1, point cp2, point end)
   {
   }
}
