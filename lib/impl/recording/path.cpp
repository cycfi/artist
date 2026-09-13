/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include "recording_impl.hpp"
#include <infra/support.hpp>
#include <algorithm>
#include <cmath>

namespace cycfi::artist::recording
{
   namespace
   {
      constexpr int  arc_segments_per_turn = 64;
      constexpr int  curve_segments = 16;

      enum op_code : int
      {
         op_move, op_line, op_close, op_arc, op_arc_to,
         op_quad, op_cubic, op_rect, op_round_rect, op_append
      };

      void push_op(std::vector<float>& ops, op_code code, std::initializer_list<float> args)
      {
         ops.push_back(float(code));
         ops.insert(ops.end(), args.begin(), args.end());
      }
   }

   ////////////////////////////////////////////////////////////////////////////
   // Geometry helpers
   rect transform_bounds(affine_transform const& m, rect const& r)
   {
      point c[4] =
      {
         m.apply(point{r.left, r.top}),     m.apply(point{r.right, r.top}),
         m.apply(point{r.right, r.bottom}), m.apply(point{r.left, r.bottom})
      };
      rect b{c[0].x, c[0].y, c[0].x, c[0].y};
      for (int i = 1; i != 4; ++i)
      {
         b.left = std::min(b.left, c[i].x);
         b.top = std::min(b.top, c[i].y);
         b.right = std::max(b.right, c[i].x);
         b.bottom = std::max(b.bottom, c[i].y);
      }
      return b;
   }

   rect intersect(rect const& a, rect const& b)
   {
      rect r{
         std::max(a.left, b.left), std::max(a.top, b.top),
         std::min(a.right, b.right), std::min(a.bottom, b.bottom)
      };
      if (r.right <= r.left || r.bottom <= r.top)
         return {};
      return r;
   }

   ////////////////////////////////////////////////////////////////////////////
   // path_impl
   bool path_impl::drawable() const
   {
      for (auto const& s : subpaths)
         if (s.pts.size() >= 2)
            return true;
      return false;
   }

   void path_impl::clear()
   {
      subpaths.clear();
      ops.clear();
      start = current = {};
      has_current = false;
   }

   void path_impl::set_transform(affine_transform const& m)
   {
      // Re-express the current point in the new user space, so a later
      // arc_to or curve starts from where the path actually is.
      auto inv = m.invert();
      current = inv.apply(xf.apply(current));
      start = inv.apply(xf.apply(start));
      xf = m;
   }

   void path_impl::emit_move(point p)
   {
      subpaths.push_back({{xf.apply(p)}, false});
      start = current = p;
      has_current = true;
   }

   void path_impl::emit_line(point p)
   {
      if (!has_current)
      {
         emit_move(p);
         return;
      }
      // After a close the path continues in a new subpath from the start of
      // the closed one.
      if (subpaths.empty() || subpaths.back().closed)
         subpaths.push_back({{xf.apply(current)}, false});
      subpaths.back().pts.push_back(xf.apply(p));
      current = p;
   }

   void path_impl::emit_close()
   {
      if (!subpaths.empty() && !subpaths.back().pts.empty())
      {
         subpaths.back().closed = true;
         current = start;
      }
   }

   void path_impl::emit_arc(point c, float r, float a0, float a1, bool ccw)
   {
      r = std::max(r, 0.0f);
      double sweep = double(a1) - a0;
      if (!ccw)
      {
         while (sweep < 0)
            sweep += 2 * pi;
      }
      else
      {
         while (sweep > 0)
            sweep -= 2 * pi;
      }

      int n = std::max(2, int(std::ceil(std::abs(sweep) / (2 * pi) * arc_segments_per_turn)));
      point s{float(c.x + r * std::cos(a0)), float(c.y + r * std::sin(a0))};
      if (has_current)
         emit_line(s);
      else
         emit_move(s);

      for (int i = 1; i <= n; ++i)
      {
         double a = a0 + sweep * i / n;
         emit_line({float(c.x + r * std::cos(a)), float(c.y + r * std::sin(a))});
      }
   }

   void path_impl::move_to(point p)
   {
      push_op(ops, op_move, {p.x, p.y});
      emit_move(p);
   }

   void path_impl::line_to(point p)
   {
      push_op(ops, op_line, {p.x, p.y});
      emit_line(p);
   }

   void path_impl::close()
   {
      push_op(ops, op_close, {});
      emit_close();
   }

   void path_impl::arc(point c, float r, float a0, float a1, bool ccw)
   {
      push_op(ops, op_arc, {c.x, c.y, r, a0, a1, ccw? 1.0f : 0.0f});
      emit_arc(c, r, a0, a1, ccw);
   }

   void path_impl::arc_to(point p1, point p2, float radius)
   {
      push_op(ops, op_arc_to, {p1.x, p1.y, p2.x, p2.y, radius});

      // Adapted from http://code.google.com/p/fxcanvas/, as the other
      // backends have it.
      if (!has_current)
      {
         emit_move(p1);
         return;
      }
      if (radius == 0)
      {
         emit_line(p1);
         return;
      }

      double a1 = current.y - p1.y;
      double b1 = current.x - p1.x;
      double a2 = p2.y - p1.y;
      double b2 = p2.x - p1.x;
      double mm = std::fabs(a1 * b2 - b1 * a2);

      if (mm < 1.0e-8)
      {
         emit_line(p1);
         return;
      }

      double dd = a1 * a1 + b1 * b1;
      double cc = a2 * a2 + b2 * b2;
      double tt = a1 * a2 + b1 * b2;
      double k1 = radius * std::sqrt(dd) / mm;
      double k2 = radius * std::sqrt(cc) / mm;
      double j1 = k1 * tt / dd;
      double j2 = k2 * tt / cc;
      double cx = k1 * b2 + k2 * b1;
      double cy = k1 * a2 + k2 * a1;
      double px = b1 * (k2 + j1);
      double py = a1 * (k2 + j1);
      double qx = b2 * (k1 + j2);
      double qy = a2 * (k1 + j2);
      double start_angle = std::atan2(py - cy, px - cx);
      double end_angle = std::atan2(qy - cy, qx - cx);
      bool ccw = (b1 * a2 > b2 * a1);

      emit_arc(
         {float(cx + p1.x), float(cy + p1.y)},
         radius, float(start_angle), float(end_angle), ccw
      );
   }

   void path_impl::quad_to(point cp, point end)
   {
      push_op(ops, op_quad, {cp.x, cp.y, end.x, end.y});
      if (!has_current)
         emit_move(cp);
      point p0 = current;
      for (int i = 1; i <= curve_segments; ++i)
      {
         float t = float(i) / curve_segments;
         float u = 1 - t;
         emit_line({
            u * u * p0.x + 2 * u * t * cp.x + t * t * end.x,
            u * u * p0.y + 2 * u * t * cp.y + t * t * end.y
         });
      }
   }

   void path_impl::cubic_to(point cp1, point cp2, point end)
   {
      push_op(ops, op_cubic, {cp1.x, cp1.y, cp2.x, cp2.y, end.x, end.y});
      if (!has_current)
         emit_move(cp1);
      point p0 = current;
      for (int i = 1; i <= curve_segments; ++i)
      {
         float t = float(i) / curve_segments;
         float u = 1 - t;
         float b0 = u * u * u, b1 = 3 * u * u * t, b2 = 3 * u * t * t, b3 = t * t * t;
         emit_line({
            b0 * p0.x + b1 * cp1.x + b2 * cp2.x + b3 * end.x,
            b0 * p0.y + b1 * cp1.y + b2 * cp2.y + b3 * end.y
         });
      }
   }

   void path_impl::add_rect(rect const& r)
   {
      push_op(ops, op_rect, {r.left, r.top, r.right, r.bottom});
      emit_move({r.left, r.top});
      emit_line({r.right, r.top});
      emit_line({r.right, r.bottom});
      emit_line({r.left, r.bottom});
      emit_close();
   }

   void path_impl::add_round_rect(rect const& r, float radius)
   {
      push_op(ops, op_round_rect, {r.left, r.top, r.right, r.bottom, radius});

      // A new subpath: the first corner starts it rather than joining it to
      // whatever came before.
      has_current = false;
      constexpr auto a = float(pi / 180.0);
      emit_arc({r.right - radius, r.top + radius}, radius, -90 * a, 0 * a, false);
      emit_arc({r.right - radius, r.bottom - radius}, radius, 0 * a, 90 * a, false);
      emit_arc({r.left + radius, r.bottom - radius}, radius, 90 * a, 180 * a, false);
      emit_arc({r.left + radius, r.top + radius}, radius, 180 * a, 270 * a, false);
      emit_close();
   }

   void path_impl::append(path_impl const& other)
   {
      ops.push_back(float(op_append));
      ops.insert(ops.end(), other.ops.begin(), other.ops.end());

      // Points come back to the other path's user space, then into ours.
      auto back = other.xf.invert();
      for (auto const& s : other.subpaths)
      {
         subpath t;
         t.closed = s.closed;
         t.pts.reserve(s.pts.size());
         for (auto const& q : s.pts)
            t.pts.push_back(xf.apply(back.apply(q)));
         subpaths.push_back(std::move(t));
      }
      if (other.has_current)
      {
         current = other.current;
         start = other.start;
         has_current = true;
      }
   }

   rect path_impl::bounds() const
   {
      bool any = false;
      rect b;
      for (auto const& s : subpaths)
      {
         for (auto const& p : s.pts)
         {
            if (!any)
            {
               b = {p.x, p.y, p.x, p.y};
               any = true;
            }
            else
            {
               b.left = std::min(b.left, p.x);
               b.top = std::min(b.top, p.y);
               b.right = std::max(b.right, p.x);
               b.bottom = std::max(b.bottom, p.y);
            }
         }
      }
      return any? b : rect{};
   }

   bool path_impl::includes(point p) const
   {
      // Winding number over every subpath taken as closed, as a fill does.
      // Odd winding numbers are inside under the even-odd rule too.
      int wn = 0;
      for (auto const& s : subpaths)
      {
         auto n = s.pts.size();
         if (n < 2)
            continue;
         for (std::size_t i = 0; i != n; ++i)
         {
            auto a = s.pts[i];
            auto b = s.pts[(i + 1) % n];
            float side = (b.x - a.x) * (p.y - a.y) - (p.x - a.x) * (b.y - a.y);
            if (a.y <= p.y)
            {
               if (b.y > p.y && side > 0)
                  ++wn;
            }
            else
            {
               if (b.y <= p.y && side < 0)
                  --wn;
            }
         }
      }
      return rule == artist::path::fill_winding? wn != 0 : (wn & 1) != 0;
   }
}

namespace cycfi::artist
{
   using recording::path_impl;

   path::path()
    : _impl(new path_impl)
   {
   }

   path::~path()
   {
      delete _impl;
   }

   path::path(path const& rhs)
    : _impl(rhs._impl? new path_impl(*rhs._impl) : nullptr)
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
         _impl = rhs._impl? new path_impl(*rhs._impl) : nullptr;
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
      if (_impl == rhs._impl)
         return true;
      if (!_impl || !rhs._impl)
         return false;
      return _impl->rule == rhs._impl->rule && _impl->ops == rhs._impl->ops;
   }

   bool path::is_empty() const
   {
      return !_impl || _impl->empty();
   }

   bool path::includes(point p) const
   {
      return _impl && _impl->includes(p);
   }

   rect path::bounds() const
   {
      return _impl? _impl->bounds() : rect{};
   }

   void path::close()
   {
      if (_impl)
         _impl->close();
   }

   void path::add_rect(rect const& r)
   {
      if (_impl)
         _impl->add_rect(r);
   }

   void path::move_to(point p)
   {
      if (_impl)
         _impl->move_to(p);
   }

   void path::line_to(point p)
   {
      if (_impl)
         _impl->line_to(p);
   }

   void path::arc_to(point p1, point p2, float radius)
   {
      if (_impl)
         _impl->arc_to(p1, p2, radius);
   }

   void path::arc(point p, float radius, float start_angle, float end_angle, bool ccw)
   {
      if (_impl)
         _impl->arc(p, radius, start_angle, end_angle, ccw);
   }

   void path::quadratic_curve_to(point cp, point end)
   {
      if (_impl)
         _impl->quad_to(cp, end);
   }

   void path::bezier_curve_to(point cp1, point cp2, point end)
   {
      if (_impl)
         _impl->cubic_to(cp1, cp2, end);
   }

   void path::fill_rule(fill_rule_enum rule)
   {
      if (_impl)
         _impl->rule = rule;
   }

   void path::add_round_rect_impl(rect const& r, float radius)
   {
      if (_impl)
         _impl->add_round_rect(r, radius);
   }
}
