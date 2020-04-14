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

      if (_generators.empty())
         return nullptr;
      if (_generators.size() == 1)
         return _generators[0](mode);

      if (_fill_geom)
         return _fill_geom;

      clear();
      for (auto const& gen : _generators)
         _geometries.push_back(gen(mode));
      _fill_geom = make_group(_geometries, _mode);
      return _fill_geom;
   }

   void path_impl::clear()
   {
      for (auto& g : _geometries)
         release(g);
      _geometries.clear();
      _generators.clear();
      release(_fill_geom);
   }

   void path_impl::fill(
      d2d_canvas& cnv
    , d2d_paint* paint
    , bool preserve)
   {
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
      if (!empty())
      {
         auto mode = stroke_mode;
         for (auto const& gen : _generators)
         {
            auto p = gen(mode);
            _geometries.push_back(p);
            cnv.DrawGeometry(p, paint, line_width, nullptr);
         }
         if (!preserve)
            clear();
      }
   }

   void path_impl::add(
      point p, float radius
    , float start_angle, float end_angle
    , bool ccw
   )
   {
      if (start_angle == std::fmod(end_angle, 2 * pi))
      {
         add(circle{ p, radius });
         return;
      }

      auto gen =
         [=](auto mode)
         {
            auto path = make_path();
            auto sink = start(path);
            make_arc(
               sink, p, radius
             , start_angle, end_angle
             , ccw, mode
            );
            stop(sink);
            return path;
         };

      add_gen(gen);
   }

  void make_arc(
      d2d_path_sink* sink
    , point p, float radius
    , float start_angle, float end_angle
    , bool ccw
    , path_impl::fill_type mode
   )
   {
      if (mode != path_impl::stroke_mode)
         sink->SetFillMode(d2d_fill_mode(mode));

      d2d_figure_begin flag =
         mode == path_impl::stroke_mode?
         D2D1_FIGURE_BEGIN_HOLLOW : D2D1_FIGURE_BEGIN_FILLED
         ;
      // sink->BeginFigure({ 0, 0 }, flag);

      // d2d_arc_segment arc;
      // arc.point = { p.x, p.y };
      // arc.size = { radius, radius };
      // arc.rotationAngle = (end_angle - start_angle) * 180 / pi;
      // arc.sweepDirection = ccw? d2d_ccw : d2d_cw;
      // arc.arcSize = ((end_angle - start_angle) > (2 * pi))?
      //    d2d_arc_large : d2d_arc_small;
      // sink->AddArc(arc);

      auto startx = p.x + (radius * std::cos(start_angle));
      auto starty = p.y + (radius * std::sin(start_angle));
      auto endx = p.x + (radius * std::cos(end_angle));
      auto endy = p.y + (radius * std::sin(end_angle));



      sink->BeginFigure({ startx, starty }, flag);

      d2d_arc_segment arc;
      arc.point = { endx, endy };
      arc.size = { radius, radius };
      arc.rotationAngle = (end_angle - start_angle) * 180 / pi;
      arc.sweepDirection = ccw? d2d_ccw : d2d_cw;
      arc.arcSize = d2d_arc_large;
      sink->AddArc(arc);

      sink->EndFigure(D2D1_FIGURE_END_OPEN);
   }
}

