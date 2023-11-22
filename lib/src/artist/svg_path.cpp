/*=============================================================================
   Copyright (c) 2016-2023 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <artist/path.hpp>
#include <cctype>
#include <cstdlib>
#include <cmath>
#include <string>
#include <stdexcept>

namespace cycfi::artist
{
   namespace
   {
      // Arc calculation code based on canvg (https://code.google.com/p/canvg/)
      // Ported to C++ by Joel de Gzman from C port by Mikko Mononen:
      // https://github.com/memononen/nanosvg/blob/master/src/nanosvg.h
      // Copyright (c) 2013-14 Mikko Mononen memon@inside.org

      float sqr(float x) { return x*x; }
      float vmag(float x, float y) { return sqrtf(x*x + y*y); }

      void xform_point(float& dx, float& dy, float x, float y, float* t)
      {
         dx = x*t[0] + y*t[2] + t[4];
         dy = x*t[1] + y*t[3] + t[5];
      }

      void xform_vec(float& dx, float& dy, float x, float y, float* t)
      {
         dx = x*t[0] + y*t[2];
         dy = x*t[1] + y*t[3];
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

         float rx    = fabsf(radius.x);         // x radius
         float ry    = fabsf(radius.y);         // y radius
         float rotx  = rotx_ / 180.0f * pi;	   // x rotation angle
         bool  fa    = large_arc;	            // Large arc
         bool  fs    = sweep;	                  // Sweep direction
         float x1    = p.x;							// start point
         float y1    = p.y;
         float x2    = end.x;
         float y2    = end.y;

         float dx = x1 - x2;
         float dy = y1 - y2;
         float d = sqrtf(dx*dx + dy*dy);
         if (d < 1e-6f || rx < 1e-6f || ry < 1e-6f)
         {
            // The arc degenerates to a line
            path_.line_to(x2, y2);
            p.x = x2;
            p.y = y2;
            return;
         }

         float sinrx = sinf(rotx);
         float cosrx = cosf(rotx);

         // Convert to center point parameterization.
         // http://www.w3.org/TR/SVG11/implnote.html#ArcImplementationNotes
         // 1) Compute x1', y1'
         float x1p = cosrx * dx / 2.0f + sinrx * dy / 2.0f;
         float y1p = -sinrx * dx / 2.0f + cosrx * dy / 2.0f;
         d = sqr(x1p)/sqr(rx) + sqr(y1p)/sqr(ry);
         if (d > 1)
         {
            d = sqrtf(d);
            rx *= d;
            ry *= d;
         }
         // 2) Compute cx', cy'
         float s = 0.0f;
         float sa = sqr(rx)*sqr(ry) - sqr(rx)*sqr(y1p) - sqr(ry)*sqr(x1p);
         float sb = sqr(rx)*sqr(y1p) + sqr(ry)*sqr(x1p);
         if (sa < 0.0f) sa = 0.0f;
         if (sb > 0.0f)
            s = sqrtf(sa / sb);
         if (fa == fs)
            s = -s;
         float cxp = s * rx * y1p / ry;
         float cyp = s * -ry * x1p / rx;

         // 3) Compute cx,cy from cx',cy'
         float cx = (x1 + x2)/2.0f + cosrx*cxp - sinrx*cyp;
         float cy = (y1 + y2)/2.0f + sinrx*cxp + cosrx*cyp;

         // 4) Calculate theta1, and delta theta.
         float ux = (x1p - cxp) / rx;
         float uy = (y1p - cyp) / ry;
         float vx = (-x1p - cxp) / rx;
         float vy = (-y1p - cyp) / ry;
         float a1 = vecang(1.0f,0.0f, ux,uy);	// Initial angle
         float da = vecang(ux,uy, vx,vy);		// Delta angle

         // if (vecrat(ux,uy,vx,vy) <= -1.0f) da = pi;
         // if (vecrat(ux,uy,vx,vy) >= 1.0f) da = 0;

         if (fs == 0 && da > 0)
            da -= 2 * pi;
         else if (fs == 1 && da < 0)
            da += 2 * pi;

         // Approximate the arc using cubic spline segments.
         float t[6] = {cosrx, sinrx, -sinrx, cosrx, cx, cy};

         // Split arc into max 90 degree segments.
         // The loop assumes an iteration per end point
         // (including start and end), this +1.
         int ndivs = int(fabsf(da) / (pi*0.5f) + 1.0f);
         float hda = (da / (float)ndivs) / 2.0f;
         float kappa = fabsf(4.0f / 3.0f * (1.0f - cosf(hda)) / sinf(hda));
         if (da < 0.0f)
            kappa = -kappa;

         float px = 0, py = 0;
         float ptanx = 0, ptany = 0;
         for (int i = 0; i <= ndivs; i++)
         {
            float a = a1 + da * ((float)i/(float)ndivs);
            dx = cosf(a);
            dy = sinf(a);
            float x, y;
            xform_point(x, y, dx*rx, dy*ry, t); // position
            float tanx, tany;
            xform_vec(tanx, tany, -dy*rx * kappa, dx*ry * kappa, t); // tangent
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

      void skip_to_next(char const*& s)
      {
         for (char c = *s; (c = *s) != 0; ++s)
         {
            if (!std::isspace(c) && c != ',')
               break;
         }
      }

      bool coord(char const*& s, float& coord, bool abs = false, float base = 0)
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
      }

      bool flag(char const*& s, bool& flag_)
      {
         skip_to_next(s);
         char* end;
         flag_ = std::strtol(s, &end, 10) != 0;
         if (s == end)
            return false;
         s = end;
         return true;
      };

      struct path_builder
      {
         path& _path;
         point p, prev_cp, prev_qp;

         path_builder(path& path_)
          : _path{path_}
         {}

         void move_to(char const*& s, bool abs)
         {
            point m;
            if (coord(s, m.x, abs, p.x) && coord(s, m.y, abs, p.y))
            {
               _path.move_to(m);
               p = prev_qp = prev_cp = m;
            }
         }

         void line_to(char const*& s, bool abs)
         {
            point l;
            if (coord(s, l.x, abs, p.x) && coord(s, l.y, abs, p.y))
            {
               _path.line_to(l);
               p = prev_qp = prev_cp = l;
            }
         }

         void line_to_h(char const*& s, bool abs)
         {
            float lx;
            if (coord(s, lx, abs, p.x))
            {
               _path.line_to(lx, p.y);
               p.x = prev_qp.x = prev_cp.x = lx;
            }
         }

         void line_to_v(char const*& s, bool abs)
         {
            float ly;
            if (coord(s, ly, abs, p.y))
            {
               _path.line_to(p.x, ly);
               p.y = prev_qp.y = prev_cp.y = ly;
            }
         }

         void bezier_curve_to(char const*& s, bool abs)
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
                  _path.bezier_curve_to(cp1, cp2, end);
                  p = prev_qp = end;
                  prev_cp = p.reflect(cp2);
               }
            }

         void shorthand_bezier_curve_to(char const*& s, bool abs)
         {
            point cp2, end;
            if (
               coord(s, cp2.x, abs, p.x) &&
               coord(s, cp2.y, abs, p.y) &&
               coord(s, end.x, abs, p.x) &&
               coord(s, end.y, abs, p.y))
            {
               _path.bezier_curve_to(prev_cp, cp2, end);
               p = prev_qp = end;
               prev_cp = p.reflect(cp2);
            }
         }

         void quadratic_curve_to(char const*& s, bool abs)
         {
            point cp, end;
            if (
               coord(s, cp.x, abs, p.x) &&
               coord(s, cp.y, abs, p.y) &&
               coord(s, end.x, abs, p.x) &&
               coord(s, end.y, abs, p.y))
            {
               _path.quadratic_curve_to(cp, end);
               p = prev_cp = end;
               prev_qp = p.reflect(cp);
            }
         }

         void shorthand_quadratic_curve_to(char const*& s, bool abs)
         {
            point end;
            if (
               coord(s, end.x, abs, p.x) &&
               coord(s, end.y, abs, p.y))
            {
               _path.quadratic_curve_to(prev_qp, end);
               p = prev_cp = end;
               prev_qp = p.reflect(prev_qp);
            }
         }

         void arc(char const*& s, bool abs)
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
               arc_to_curve(_path, p, radius, rotx, large_arc, sweep, end);
            }
         }

         void command(char const*& s, char& cmd, bool& abs)
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
         }

         void dispatch(char const*& s, char cmd, bool abs)
         {
            switch (cmd)
            {
               case 'M': move_to(s, abs); break;
               case 'L': line_to(s, abs); break;
               case 'H': line_to_h(s, abs); break;
               case 'V': line_to_v(s, abs); break;
               case 'C': bezier_curve_to(s, abs); break;
               case 'S': shorthand_bezier_curve_to(s, abs); break;
               case 'Q': quadratic_curve_to(s, abs); break;
               case 'T': shorthand_quadratic_curve_to(s, abs); break;
               case 'A': arc(s, abs); break;
               case 'Z': _path.close(); break;

               default:
                  throw std::runtime_error{
                     std::string{"Error: Invalid SVG path syntax here: "} + s
                  };
            }
         }
      };
   }

   path::path(std::string_view svg_def)
    : path()
   {
      // Note: Why are we converting to string and not just iterate over the
      // characters in the string_view directly? Well, unfortunately, that's
      // because std::strtof requires a null-terminated string which only
      // std::string.c_str() can provide. There is no guarantee that a
      // std::string_view is null-terminated and std::strtof can read past
      // the string-view's end which is undefined behavior. I wish I could
      // just use Boost.Spirit!
      auto str = std::string{svg_def.data(), svg_def.size()};
      auto s = str.c_str();
      bool abs; char cmd;

      path_builder builder{*this};
      while (*s)
      {
         skip_to_next(s);
         builder.command(s, cmd, abs);
         builder.dispatch(s, cmd, abs);
      }
   }
}
