/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <path_impl.hpp>

namespace cycfi::artist
{
   d2d_geometry* path_impl::compute_fill()
   {
      auto mode = fill_type(_mode);

      if (_geom_gens.empty())
         return nullptr;
      if (_geom_gens.size() == 1)
         return _geom_gens[0](mode);

      if (_fill_geom)
         return _fill_geom;

      clear();
      for (auto const& gen : _geom_gens)
         _geometries.push_back(gen(mode));
      _fill_geom = make_group(_geometries, _mode);
      return _fill_geom;
   }

   void path_impl::clear()
   {
      for (auto& g : _geometries)
         release(g);
      _geometries.clear();
      _geom_gens.clear();
      release(_fill_geom);
   }

   void path_impl::fill(
      d2d_canvas& cnv
    , d2d_paint* paint
    , bool preserve)
   {
      build_path();
      if (!empty())
      {
         cnv.FillGeometry(compute_fill(), paint, nullptr);
         if (!preserve)
            clear();
      }
   }

   void path_impl::stroke(
      d2d_canvas& cnv
    , d2d_paint* paint
    , float line_width
    , bool preserve)
   {
      build_path();
      if (!empty())
      {
         auto mode = stroke_mode;
         for (auto const& gen : _geom_gens)
         {
            auto p = gen(mode);
            _geometries.push_back(p);
            cnv.DrawGeometry(p, paint, line_width, nullptr);
         }
         if (!preserve)
            clear();
      }
   }

   void path_impl::begin_path()
   {
      if (_path_gens_state == path_started)
         end_path();
      _path_gens_state = path_started;
      _path_gens.push_back(
         [](d2d_path_sink* sink, fill_type mode)
         {
            d2d_figure_begin flag =
               mode == path_impl::stroke_mode?
               d2d_path_hollow : d2d_path_filled
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
         [close](d2d_path_sink* sink, fill_type mode)
         {
            sink->EndFigure(
               close? d2d_path_closed : d2d_path_open
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
         [p](d2d_path_sink* sink, fill_type mode)
         {
            d2d_figure_begin flag =
               mode == path_impl::stroke_mode?
               d2d_path_hollow : d2d_path_filled
               ;
            sink->BeginFigure({ p.x, p.y }, flag);
         }
      );
      _cp = p;
   }

   void path_impl::line_to(point p)
   {
      _path_gens.push_back(
         [p](d2d_path_sink* sink, fill_type mode)
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
      move_to({ startx, starty });

      auto diff_angle = end_angle - start_angle;
      if (diff_angle < 0)
         diff_angle += 2 * pi;

      d2d_arc_segment arc;
      arc.point = { endx, endy };
      arc.size = { radius, radius };
      arc.rotationAngle = diff_angle * 180 / pi;
      arc.sweepDirection = ccw? d2d_ccw : d2d_cw;
      arc.arcSize = diff_angle > pi? d2d_arc_large : d2d_arc_small;

      _path_gens.push_back(
         [=](d2d_path_sink* sink, fill_type mode)
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

      line_to({ px + p1.x, py + p1.y });

      arc(
         { cx + p1.x, cy + p1.y }
       , radius, start_angle, end_angle
       , (b1 * a2 > b2 * a1)
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

