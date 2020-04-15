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
   }

   void path_impl::line_to(point p)
   {
      _path_gens.push_back(
         [p](d2d_path_sink* sink, fill_type mode)
         {
            sink->AddLine({ p.x, p.y });
         }
      );
   }

   void path_impl::arc(
      point p, float radius
    , float start_angle, float end_angle
    , bool ccw
   )
   {
      auto startx = p.x + (radius * std::cos(start_angle));
      auto starty = p.y + (radius * std::sin(start_angle));
      move_to({ startx, starty });

      _path_gens.push_back(
         [=](d2d_path_sink* sink, fill_type mode)
         {
            auto endx = p.x + (radius * std::cos(end_angle));
            auto endy = p.y + (radius * std::sin(end_angle));

            d2d_arc_segment arc;
            arc.point = { endx, endy };
            arc.size = { radius, radius };
            arc.rotationAngle = (end_angle - start_angle) * 180 / pi;
            arc.sweepDirection = ccw? d2d_ccw : d2d_cw;
            arc.arcSize = d2d_arc_large;
            sink->AddArc(arc);
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

