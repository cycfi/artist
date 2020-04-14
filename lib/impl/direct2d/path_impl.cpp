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
         clear();
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
//      auto path = make_path();
//      auto sink = start(path);
//      //make_arc(sink, el);
//      stop(sink);
//      add(path);
   }

//   void make_arc(d2d_path_sink* sink, pe_arc const& arc)
//   {
//   }
}

