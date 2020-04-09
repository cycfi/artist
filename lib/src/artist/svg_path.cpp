/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <artist/path.hpp>
#include <cctype>
#include <cstdlib>
#include <cmath>
#include <string>

namespace cycfi::artist
{
   namespace
   {
      // Arc calculation code based on canvg (https://code.google.com/p/canvg/)
      // Ported to C++ from C port by Mikko Mononen:
      // https://github.com/memononen/nanosvg/blob/master/src/nanosvg.h
      // Copyright (c) 2013-14 Mikko Mononen memon@inside.org

      constexpr auto pi = 3.14159265358979323846264338327f;

      float sqr(float x) { return x*x; }
      float vmag(float x, float y) { return sqrtf(x*x + y*y); }

      void xform_point(float* dx, float* dy, float x, float y, float* t)
      {
         *dx = x*t[0] + y*t[2] + t[4];
         *dy = x*t[1] + y*t[3] + t[5];
      }

      void xform_vec(float* dx, float* dy, float x, float y, float* t)
      {
         *dx = x*t[0] + y*t[2];
         *dy = x*t[1] + y*t[3];
      }

      float vecrat(float ux, float uy, float vx, float vy)
      {
         return (ux*vx + uy*vy) / (vmag(ux,uy) * vmag(vx,vy));
      }

      float vecang(float ux, float uy, float vx, float vy)
      {
         float r = vecrat(ux,uy, vx,vy);
         if (r < -1.0f) r = -1.0f;
         if (r > 1.0f) r = 1.0f;
         return ((ux*vy < uy*vx) ? -1.0f : 1.0f) * acosf(r);
      }

      void arc_to_curve(
         path& path_
       , point& p, point radius, float rotx_
       , bool large_arc, bool sweep, point end
      )
      {
         // Ported from canvg (https://code.google.com/p/canvg/)
         float cx, cy, dx, dy, d;
         float x1p, y1p, cxp, cyp, s, sa, sb;
         float ux, uy, vx, vy, a1, da;
         float x, y, tanx, tany, a, px = 0, py = 0, ptanx = 0, ptany = 0, t[6];
         float sinrx, cosrx;
         int i, ndivs;
         float hda, kappa;

         float rx = fabsf(radius.x);            // x radius
         float ry = fabsf(radius.y);            // y radius
         float rotx = rotx_ / 180.0f * pi;	   // x rotation angle
         bool fa = large_arc;	                  // Large arc
         bool fs = sweep;	                     // Sweep direction
         float x1 = p.x;							   // start point
         float y1 = p.y;
         float x2 = end.x;
         float y2 = end.y;

         dx = x1 - x2;
         dy = y1 - y2;
         d = sqrtf(dx*dx + dy*dy);
         if (d < 1e-6f || rx < 1e-6f || ry < 1e-6f) {
            // The arc degenerates to a line
            path_.line_to(x2, y2);
            p.x = x2;
            p.y = y2;
            return;
         }

         sinrx = sinf(rotx);
         cosrx = cosf(rotx);

         // Convert to center point parameterization.
         // http://www.w3.org/TR/SVG11/implnote.html#ArcImplementationNotes
         // 1) Compute x1', y1'
         x1p = cosrx * dx / 2.0f + sinrx * dy / 2.0f;
         y1p = -sinrx * dx / 2.0f + cosrx * dy / 2.0f;
         d = sqr(x1p)/sqr(rx) + sqr(y1p)/sqr(ry);
         if (d > 1) {
            d = sqrtf(d);
            rx *= d;
            ry *= d;
         }
         // 2) Compute cx', cy'
         s = 0.0f;
         sa = sqr(rx)*sqr(ry) - sqr(rx)*sqr(y1p) - sqr(ry)*sqr(x1p);
         sb = sqr(rx)*sqr(y1p) + sqr(ry)*sqr(x1p);
         if (sa < 0.0f) sa = 0.0f;
         if (sb > 0.0f)
            s = sqrtf(sa / sb);
         if (fa == fs)
            s = -s;
         cxp = s * rx * y1p / ry;
         cyp = s * -ry * x1p / rx;

         // 3) Compute cx,cy from cx',cy'
         cx = (x1 + x2)/2.0f + cosrx*cxp - sinrx*cyp;
         cy = (y1 + y2)/2.0f + sinrx*cxp + cosrx*cyp;

         // 4) Calculate theta1, and delta theta.
         ux = (x1p - cxp) / rx;
         uy = (y1p - cyp) / ry;
         vx = (-x1p - cxp) / rx;
         vy = (-y1p - cyp) / ry;
         a1 = vecang(1.0f,0.0f, ux,uy);	// Initial angle
         da = vecang(ux,uy, vx,vy);		// Delta angle

         // if (vecrat(ux,uy,vx,vy) <= -1.0f) da = pi;
         // if (vecrat(ux,uy,vx,vy) >= 1.0f) da = 0;

         if (fs == 0 && da > 0)
            da -= 2 * pi;
         else if (fs == 1 && da < 0)
            da += 2 * pi;

         // Approximate the arc using cubic spline segments.
         t[0] = cosrx; t[1] = sinrx;
         t[2] = -sinrx; t[3] = cosrx;
         t[4] = cx; t[5] = cy;

         // Split arc into max 90 degree segments.
         // The loop assumes an iteration per end point (including start and end), this +1.
         ndivs = (int)(fabsf(da) / (pi*0.5f) + 1.0f);
         hda = (da / (float)ndivs) / 2.0f;
         kappa = fabsf(4.0f / 3.0f * (1.0f - cosf(hda)) / sinf(hda));
         if (da < 0.0f)
            kappa = -kappa;

         for (i = 0; i <= ndivs; i++) {
            a = a1 + da * ((float)i/(float)ndivs);
            dx = cosf(a);
            dy = sinf(a);
            xform_point(&x, &y, dx*rx, dy*ry, t); // position
            xform_vec(&tanx, &tany, -dy*rx * kappa, dx*ry * kappa, t); // tangent
            if (i > 0)
               path_.bezier_curve_to(px+ptanx, py+ptany, x-tanx, y-tany, x, y);
            px = x;
            py = y;
            ptanx = tanx;
            ptany = tany;
         }

         p.x = x2;
         p.y = y2;
      }
   }

   path::path(std::string_view svg_def)
    : path()
   {
      point p, prev_cp, prev_qp;

      auto skip_to_next =
         [](auto& s) -> void
         {
            for (char c = *s; (c = *s) != 0; ++s)
            {
               if (!std::isspace(c) && c != ',')
                  break;
            }
         };

      auto coord =
         [&](char const*& s, float& coord, bool abs = false, float base = 0) -> bool
         {
            skip_to_next(s);
            char* end;
            coord = std::strtof(s, &end);
            if (s == end)
               return false;
            s = end;
            if (!abs)
               coord += base;
            return true;
         };

      auto flag =
         [&](auto& s, bool& flag_) -> bool
         {
            skip_to_next(s);
            char* end;
            flag_ = std::strtol(s, &end, 10) != 0;
            if (s == end)
               return false;
            s = end;
            return true;
         };

      auto move_to_ =
         [&](auto& s, bool abs) -> void
         {
            point m;
            if (coord(s, m.x, abs, p.x) && coord(s, m.y, abs, p.y))
            {
               this->move_to(m);
               p = prev_qp = prev_cp = m;
            }
         };

      auto line_to_ =
         [&](auto& s, bool abs) -> void
         {
            point l;
            if (coord(s, l.x, abs, p.x) && coord(s, l.y, abs, p.y))
            {
               this->line_to(l);
               p = prev_qp = prev_cp = l;
            }
         };

      auto line_to_h =
         [&](auto& s, bool abs) -> void
         {
            float lx;
            if (coord(s, lx, abs, p.x))
            {
               this->line_to(lx, p.y);
               p.x = prev_qp.x = prev_cp.x = lx;
            }
         };

      auto line_to_v =
         [&](auto& s, bool abs) -> void
         {
            float ly;
            if (coord(s, ly, abs, p.y))
            {
               this->line_to(p.x, ly);
               p.y = prev_qp.y = prev_cp.y = ly;
            }
         };

      auto bezier_curve_to_ =
         [&](auto& s, bool abs) -> void
         {
            point cp1, cp2, end;
            if (
               coord(s, cp1.x, abs, p.x) &&
               coord(s, cp1.y, abs, p.y) &&
               coord(s, cp2.x, abs, p.x) &&
               coord(s, cp2.y, abs, p.y) &&
               coord(s, end.x, abs, p.x) &&
               coord(s, end.y, abs, p.y))
            {
               this->bezier_curve_to(cp1, cp2, end);
               p = prev_qp = end;
               prev_cp = p.reflect(cp2);
            }
         };

      auto shorthand_bezier_curve_to_ =
         [&](auto& s, bool abs) -> void
         {
            point cp2, end;
            if (
               coord(s, cp2.x, abs, p.x) &&
               coord(s, cp2.y, abs, p.y) &&
               coord(s, end.x, abs, p.x) &&
               coord(s, end.y, abs, p.y))
            {
               this->bezier_curve_to(prev_cp, cp2, end);
               p = prev_qp = end;
               prev_cp = p.reflect(cp2);
            }
         };

      auto quadratic_curve_to_ =
         [&](auto& s, bool abs) -> void
         {
            point cp, end;
            if (
               coord(s, cp.x, abs, p.x) &&
               coord(s, cp.y, abs, p.y) &&
               coord(s, end.x, abs, p.x) &&
               coord(s, end.y, abs, p.y))
            {
               this->quadratic_curve_to(cp, end);
               p = prev_cp = end;
               prev_qp = p.reflect(cp);
            }
         };

      auto shorthand_quadratic_curve_to_ =
         [&](auto& s, bool abs) -> void
         {
            point end;
            if (
               coord(s, end.x, abs, p.x) &&
               coord(s, end.y, abs, p.y))
            {
               this->quadratic_curve_to(prev_qp, end);
               p = prev_cp = end;
               prev_qp = p.reflect(prev_qp);
            }
         };

      auto arc_ =
         [&](auto& s, bool abs) -> void
         {
            point radius, end;
            float rotx;
            bool large_arc, sweep;
            if (
               coord(s, radius.x) &&
               coord(s, radius.y) &&
               coord(s, rotx) &&
               flag(s, large_arc) &&
               flag(s, sweep) &&
               coord(s, end.x, abs, p.x) &&
               coord(s, end.y, abs, p.y))
            {
               arc_to_curve(*this, p, radius, rotx, large_arc, sweep, end);
            }
         };

      auto command =
         [&](char const*& s, char& cmd, bool& abs) -> void
         {
            auto c = std::toupper(*s);
            switch (c)
            {
               case 'M': case 'L': case 'H':
               case 'V': case 'C': case 'S':
               case 'Q': case 'T': case 'A':
               case 'Z': case 'z':
               {
                  abs = std::isupper(*s);
                  cmd = c;
                  ++s;
               }
            }
         };

      auto dispatch =
         [&](char const*& s, char cmd, bool abs)
         {
            switch (cmd)
            {
               case 'M': move_to_(s, abs); break;
               case 'L': line_to_(s, abs); break;
               case 'H': line_to_h(s, abs); break;
               case 'V': line_to_v(s, abs); break;
               case 'C': bezier_curve_to_(s, abs); break;
               case 'S': shorthand_bezier_curve_to_(s, abs); break;
               case 'Q': quadratic_curve_to_(s, abs); break;
               case 'T': shorthand_quadratic_curve_to_(s, abs); break;
               case 'A': arc_(s, abs); break;
               case 'Z': case 'z': close(); break;

               default:
                  throw std::runtime_error{
                     std::string{ "Error: Invalid SVG path syntax here: " } + s
                  };
            }
         };

      // Note: Why are we converting to string and not just iterate over the
      // characters in the string_view directly? Well, unfortunately, because
      // std::stof requires a null-terminated string which only
      // std::string.c_str() can provide. There is no guarantee that a
      // std::string_view is null-terminated. I wish I could just use
      // Boost.Spirit!
      auto str = std::string{ svg_def.data(), svg_def.end() };
      auto s = str.c_str();

      bool abs; char cmd;
      while (*s)
      {
         skip_to_next(s);
         command(s, cmd, abs);
         dispatch(s, cmd, abs);
      }
   }
}
