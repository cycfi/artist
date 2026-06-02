/*=============================================================================
   Copyright (c) 2016-2024 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <artist/path.hpp>
#include "cairo_private.hpp"
#include <infra/support.hpp>
#include <cmath>
#include <algorithm>

namespace cycfi::artist
{
   path::path()
    : _impl(new cairo_artist_path_t)
   {
   }

   path::~path()
   {
      delete _impl;
   }

   path::path(path const& rhs)
    : _impl(rhs._impl ? new cairo_artist_path_t(*rhs._impl) : nullptr)
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
         delete _impl;
         _impl = rhs._impl ? new cairo_artist_path_t(*rhs._impl) : nullptr;
      }
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

   bool path::operator==(path const& rhs) const
   {
      if (_impl == rhs._impl) return true;
      if (!_impl || !rhs._impl) return false;
      if (_impl->fill_rule != rhs._impl->fill_rule) return false;

      cairo_path_t* lp = cairo_copy_path(_impl->ctx);
      cairo_path_t* rp = cairo_copy_path(rhs._impl->ctx);

      bool equal = false;
      if (lp && rp && lp->num_data == rp->num_data)
      {
         equal = true;
         for (int i = 0; i < lp->num_data && equal; )
         {
            auto const& lh = lp->data[i].header;
            auto const& rh = rp->data[i].header;
            if (lh.type != rh.type || lh.length != rh.length) { equal = false; break; }
            for (int j = 1; j < lh.length && equal; ++j)
            {
               auto const& lpt = lp->data[i + j].point;
               auto const& rpt = rp->data[i + j].point;
               if (lpt.x != rpt.x || lpt.y != rpt.y) equal = false;
            }
            i += lh.length;
         }
      }

      if (lp) cairo_path_destroy(lp);
      if (rp) cairo_path_destroy(rp);
      return equal;
   }

   bool path::is_empty() const
   {
      if (!_impl) return true;
      auto* cp = cairo_copy_path(_impl->ctx);
      bool empty = !cp || cp->num_data == 0;
      if (cp) cairo_path_destroy(cp);
      return empty;
   }

   bool path::includes(point p) const
   {
      if (!_impl) return false;
      return cairo_in_fill(_impl->ctx, p.x, p.y);
   }

   rect path::bounds() const
   {
      if (!_impl) return {};
      double x1, y1, x2, y2;
      cairo_path_extents(_impl->ctx, &x1, &y1, &x2, &y2);
      return rect{float(x1), float(y1), float(x2), float(y2)};
   }

   void path::close()
   {
      if (_impl) cairo_close_path(_impl->ctx);
   }

   void path::add_rect(rect const& r)
   {
      if (_impl)
         cairo_rectangle(_impl->ctx, r.left, r.top, r.width(), r.height());
   }

   void path::move_to(point p)
   {
      if (_impl) cairo_move_to(_impl->ctx, p.x, p.y);
   }

   void path::line_to(point p)
   {
      if (_impl) cairo_line_to(_impl->ctx, p.x, p.y);
   }

   void path::arc_to(point p1, point p2, float radius)
   {
      // Adapted from http://code.google.com/p/fxcanvas/
      if (!_impl) return;
      if (radius == 0)
      {
         line_to(p1);
         return;
      }

      double cpx, cpy;
      cairo_get_current_point(_impl->ctx, &cpx, &cpy);

      auto a1 = cpy - p1.y;
      auto b1 = cpx - p1.x;
      auto a2 = p2.y - p1.y;
      auto b2 = p2.x - p1.x;
      auto mm = std::fabs(a1 * b2 - b1 * a2);

      if (mm < 1.0e-8)
      {
         line_to(p1);
         return;
      }

      auto dd = a1*a1 + b1*b1;
      auto cc = a2*a2 + b2*b2;
      auto tt = a1*a2 + b1*b2;
      auto k1 = radius * std::sqrt(dd) / mm;
      auto k2 = radius * std::sqrt(cc) / mm;
      auto j1 = k1 * tt / dd;
      auto j2 = k2 * tt / cc;
      auto cx = k1*b2 + k2*b1;
      auto cy = k1*a2 + k2*a1;
      auto px = b1*(k2+j1);
      auto py = a1*(k2+j1);
      auto qx = b2*(k1+j2);
      auto qy = a2*(k1+j2);
      auto start_angle = std::atan2(py - cy, px - cx);
      auto end_angle   = std::atan2(qy - cy, qx - cx);
      bool ccw = (b1*a2 > b2*a1);

      arc({float(cx + p1.x), float(cy + p1.y)},
          radius, float(start_angle), float(end_angle), ccw);
   }

   void path::arc(
      point p, float radius,
      float start_angle, float end_angle,
      bool ccw)
   {
      if (!_impl) return;
      radius = std::max(radius, 0.0f);
      if (ccw)
         cairo_arc_negative(_impl->ctx, p.x, p.y, radius, start_angle, end_angle);
      else
         cairo_arc(_impl->ctx, p.x, p.y, radius, start_angle, end_angle);
   }

   void path::quadratic_curve_to(point cp, point end)
   {
      if (!_impl) return;
      double x, y;
      cairo_get_current_point(_impl->ctx, &x, &y);
      cairo_curve_to(_impl->ctx,
         2.0/3.0 * cp.x + 1.0/3.0 * x,
         2.0/3.0 * cp.y + 1.0/3.0 * y,
         2.0/3.0 * cp.x + 1.0/3.0 * end.x,
         2.0/3.0 * cp.y + 1.0/3.0 * end.y,
         end.x, end.y
      );
   }

   void path::bezier_curve_to(point cp1, point cp2, point end)
   {
      if (_impl)
         cairo_curve_to(_impl->ctx, cp1.x, cp1.y, cp2.x, cp2.y, end.x, end.y);
   }

   void path::fill_rule(fill_rule_enum rule)
   {
      if (!_impl) return;
      _impl->fill_rule = rule;
      cairo_set_fill_rule(_impl->ctx,
         rule == fill_winding ? CAIRO_FILL_RULE_WINDING : CAIRO_FILL_RULE_EVEN_ODD);
   }

   void path::add_round_rect_impl(rect const& r, float radius)
   {
      if (!_impl) return;
      auto x = r.left, y = r.top, rr = r.right, b = r.bottom;
      constexpr auto a = cycfi::pi / 180.0;
      cairo_new_sub_path(_impl->ctx);
      cairo_arc(_impl->ctx, rr-radius, y+radius,  radius, -90*a,   0*a);
      cairo_arc(_impl->ctx, rr-radius, b-radius,  radius,   0*a,  90*a);
      cairo_arc(_impl->ctx, x+radius,  b-radius,  radius,  90*a, 180*a);
      cairo_arc(_impl->ctx, x+radius,  y+radius,  radius, 180*a, 270*a);
      cairo_close_path(_impl->ctx);
   }
}
