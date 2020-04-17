/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <path_impl.hpp>

namespace cycfi::artist::d2d
{
   geometry* path_impl::compute_fill()
   {
      if (_fill_geometry && _id == _generation)
         return _fill_geometry;
      auto mode = render_mode(_mode);

      if (_geometry_gens.empty())
         return nullptr;
      if (_geometry_gens.size() == 1)
         return _geometry_gens[0](mode);
      if (_fill_geometry)
         return _fill_geometry;

      compute_geometries(mode);
      _fill_geometry = make_group(_geometries, _mode);
      return _fill_geometry;
   }

   void path_impl::compute_geometries(render_mode mode)
   {
      if (_id == _generation)
         return;
      _id = _generation;
      clear_geometries();
      for (auto const& gen : _geometry_gens)
         _geometries.push_back(gen(mode));
   }

   void path_impl::clear_geometries()
   {
      for (auto& g : _geometries)
         release(g);
      _geometries.clear();
      release(_fill_geometry);
   }

   void path_impl::clear()
   {
      clear_geometries();
      _geometry_gens.clear();
   }

   void path_impl::fill(
      render_target& target
    , brush* paint
    , bool preserve)
   {
      build_path();
      if (!empty())
      {
         target.FillGeometry(compute_fill(), paint, nullptr);
         if (!preserve)
            clear();
      }
   }

   void path_impl::stroke(
      render_target& target
    , brush* paint
    , float line_width
    , bool preserve
    , stroke_style* stroke_style
   )
   {
      build_path();
      if (!empty())
      {
         compute_geometries(stroke_mode);
         for (auto geom : _geometries)
            target.DrawGeometry(geom, paint, line_width, stroke_style);
         if (!preserve)
            clear();
      }
   }

   rect path_impl::fill_bounds(render_target& target)
   {
      rectf d2d_bounds;
      build_path();
      if (!empty())
      {
         //matrix2x2f matrix = matrix2x2f::Identity();
         //target.GetTransform(&matrix);
         auto geom = compute_fill();
         geom->GetBounds(nullptr, &d2d_bounds);
      }
      return {
         d2d_bounds.left,
         d2d_bounds.top,
         d2d_bounds.right,
         d2d_bounds.bottom,
      };
   }

   rect path_impl::stroke_bounds(
      render_target& target
    , float line_width
    , stroke_style* stroke_style
   )
   {
      rectf d2d_bounds;
      build_path();
      if (!empty())
      {
         //matrix2x2f matrix = matrix2x2f::Identity();
         //target.GetTransform(&matrix);
         auto geom = compute_fill();
         geom->GetWidenedBounds(
            line_width, stroke_style, nullptr, &d2d_bounds
         );
      }
      return {
         d2d_bounds.left,
         d2d_bounds.top,
         d2d_bounds.right,
         d2d_bounds.bottom,
      };
   }

   void path_impl::begin_path()
   {
      if (_path_gens_state == path_started)
         end_path();
      _path_gens_state = path_started;
      _path_gens.push_back(
         [](geometry_sink* sink, render_mode mode)
         {
            figure_begin flag =
               mode == path_impl::stroke_mode ?
               figure_begin_hollow : figure_begin_filled
               ;
            sink->BeginFigure({ 0, 0 }, flag);
         }
      );
      _start = _cp = point{ 0, 0 };
   }

   void path_impl::end_path(bool close)
   {
      _path_gens_state = path_ended;
      _path_gens.push_back(
         [close](geometry_sink* sink, render_mode mode)
         {
            sink->EndFigure(
                    close ? figure_end_closed : figure_path_open
            );
         }
      );
      if (close)
         _cp = _start;
   }

   void path_impl::move_to(point p)
   {
      close_sub_path_if_open();
      _path_gens_state = path_started;
      _path_gens.push_back(
         [p](geometry_sink* sink, render_mode mode)
         {
            figure_begin flag =
               mode == path_impl::stroke_mode ?
               figure_begin_hollow : figure_begin_filled
               ;
            sink->BeginFigure({ p.x, p.y }, flag);
         }
      );
      _cp = p;
   }

   void path_impl::line_to(point p)
   {
      if (_path_gens_state == path_ended)
      {
         move_to(p);
         return;
      }

      _path_gens.push_back(
         [p](geometry_sink* sink, render_mode mode)
         {
            sink->AddLine({ p.x, p.y });
         }
      );
      _cp = p;
   }

   void path_impl::arc(
      point p, float radius
    , float start_angle, float end_angle
    , bool ccw
   )
   {
      auto startx = p.x + (radius * std::cos(start_angle));
      auto starty = p.y + (radius * std::sin(start_angle));
      auto endx = p.x + (radius * std::cos(end_angle));
      auto endy = p.y + (radius * std::sin(end_angle));

      if (_path_gens_state == path_ended)
         move_to({ startx, starty });
      else
         line_to({ startx, starty });

      auto diff_angle = end_angle - start_angle;
      if (diff_angle < 0)
         diff_angle += 2 * pi;

      arc_segment arc;
      arc.point = { endx, endy };
      arc.size = { radius, radius };
      arc.rotationAngle = diff_angle * 180 / pi;
      arc.sweepDirection = ccw ? sweep_dir_ccw : sweep_dir_cw;
      arc.arcSize = diff_angle > pi ? arc_large : arc_small;

      _path_gens.push_back(
         [arc](geometry_sink* sink, render_mode mode)
         {
            sink->AddArc(arc);
         }
      );

      _cp = { endx, endy };
   }

   void path_impl::arc_to(point p1, point p2, float radius)
   {
      // Adapted from http://code.google.com/p/fxcanvas/

      if (radius == 0)
      {
         line_to(p1);
         return;
      }

      auto a1 = _cp.y - p1.y;
      auto b1 = _cp.x - p1.x;
      auto a2 = p2.y - p1.y;
      auto b2 = p2.x - p1.x;
      auto mm = fabsf(a1 * b2 - b1 * a2);

      if (mm < 1.0e-8)
      {
         line_to(p1);
         return;
      }

      auto dd = a1 * a1 + b1 * b1;
      auto cc = a2 * a2 + b2 * b2;
      auto tt = a1 * a2 + b1 * b2;
      auto k1 = radius * std::sqrtf(dd) / mm;
      auto k2 = radius * std::sqrtf(cc) / mm;
      auto j1 = k1 * tt / dd;
      auto j2 = k2 * tt / cc;
      auto cx = k1 * b2 + k2 * b1;
      auto cy = k1 * a2 + k2 * a1;
      auto px = b1 * (k2 + j1);
      auto py = a1 * (k2 + j1);
      auto qx = b2 * (k1 + j2);
      auto qy = a2 * (k1 + j2);
      auto start_angle = std::atan2f(py - cy, px - cx);
      auto end_angle = std::atan2f(qy - cy, qx - cx);

      arc(
         { cx + p1.x, cy + p1.y }
       , radius, start_angle, end_angle
       , (b1 * a2 > b2 * a1)
      );
   }

   void path_impl::quadratic_curve_to(point cp, point end)
   {
      if (_path_gens_state == path_ended)
         move_to({ cp.x, cp.y });

      quadratic_bezier_segment quad;
      quad.point1 = { cp.x, cp.y };
      quad.point2 = { end.x, end.y };

      _path_gens.push_back(
         [quad](geometry_sink* sink, render_mode mode)
         {
            sink->AddQuadraticBezier(quad);
         }
      );
   }

   void path_impl::bezier_curve_to(point cp1, point cp2, point end)
   {
      if (_path_gens_state == path_ended)
         move_to({ cp1.x, cp1.y });

      bezier_segment bezier;
      bezier.point1 = { cp1.x, cp1.y };
      bezier.point2 = { cp2.x, cp2.y };
      bezier.point3 = { end.x, end.y };

      _path_gens.push_back(
         [bezier](geometry_sink* sink, render_mode mode)
         {
            sink->AddBezier(bezier);
         }
      );
   }

   void path_impl::build_path()
   {
      if (_path_gens.size())
      {
         close_sub_path_if_open();

         auto gen =
            [this, gen_vec = _path_gens](auto mode)
            {
               auto path = make_path();
               auto sink = start(path);

               if (mode != path_impl::stroke_mode)
                  sink->SetFillMode(fill_mode(mode));

               for (auto const& gen : gen_vec)
                  gen(sink, mode);
               stop(sink);
               return path;
            };

         add_gen(gen);
         _path_gens.clear();
      }
   }
}

