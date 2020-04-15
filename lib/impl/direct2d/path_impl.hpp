/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#if !defined(ELEMENTS_DETAIL_DIRECT_2D_PATH_IMPL_APR_13_2020)
#define ELEMENTS_DETAIL_DIRECT_2D_PATH_IMPL_APR_13_2020

#include <canvas_impl.hpp>
#include <vector>
#include <variant>

namespace cycfi::artist
{
   struct path_impl
   {
   public:

      enum fill_type
      {
         fill_winding = D2D1_FILL_MODE_WINDING,
         fill_odd_even = D2D1_FILL_MODE_ALTERNATE,
         stroke_mode = -1
      };

      using path_gen = std::function<void(d2d_path_sink* sink, fill_type mode)>;
      using path_gen_vector = std::vector<path_gen>;

      using geometry_gen = std::function<d2d_geometry*(fill_type mode)>;
      using geometry_gen_vector = std::vector<geometry_gen>;

      using geometry_vector = std::vector<d2d_geometry*>;
      using iterator = geometry_vector::iterator;

                           ~path_impl();

      bool                 empty() const;
      void                 clear();
      d2d_geometry*        compute_fill();
      void                 fill_rule(d2d_fill_mode mode);

      void                 add(rect r);
      void                 add(rect r, float radius);
      void                 add(circle c);

      void                 fill(
                              d2d_canvas& cnv
                            , d2d_paint* paint
                            , bool preserve
                           );

      void                 stroke(
                              d2d_canvas& cnv
                            , d2d_paint* paint
                            , float line_width
                            , bool preserve
                           );

      void                 begin_path();
      void                 end_path(bool close = false);
      void                 close();
      void                 move_to(point p);
      void                 line_to(point p);
      void                 arc(
                              point p, float radius
                            , float start_angle, float end_angle
                            , bool ccw
                           );

   private:

      enum path_gen_state { path_started, path_ended };

      void                 add_gen(geometry_gen&& gen);
      void                 close_sub_path_if_open();
      void                 build_path();

      geometry_vector      _geometries;
      geometry_gen_vector  _geom_gens;
      d2d_fill_mode        _mode = D2D1_FILL_MODE_WINDING;
      d2d_geometry_group*  _fill_geom = nullptr;
      path_gen_vector      _path_gens;
      path_gen_state       _path_gens_state = path_ended;
   };

   ////////////////////////////////////////////////////////////////////////////
   // Low level utils
   ////////////////////////////////////////////////////////////////////////////
   d2d_rect*               make_rect(rect r);
   d2d_round_rect*         make_round_rect(rect r, float radius);
   d2d_ellipse*            make_circle(circle c);

   d2d_path*               make_path();
   d2d_path_sink*          start(d2d_path* path);
   void                    stop(d2d_path_sink* sink);

   ////////////////////////////////////////////////////////////////////////////
   // Inlines
   ////////////////////////////////////////////////////////////////////////////
   inline path_impl::~path_impl()
   {
      for (auto& g : _geometries)
         release(g);
      release(_fill_geom);
   }

   inline bool path_impl::empty() const
   {
      return _geom_gens.empty();
   }

   inline void path_impl::add_gen(geometry_gen&& gen)
   {
      _geom_gens.emplace_back(std::move(gen));
      release(_fill_geom);
   }

   inline void path_impl::add(rect r)
   {
      add_gen([=](auto){ return make_rect(r); });
   }

   inline void path_impl::add(rect r, float radius)
   {
      add_gen([=](auto){ return make_round_rect(r, radius); });
   }

   inline void path_impl::add(circle c)
   {
      add_gen([=](auto){ return make_circle(c); });
   }

   inline void path_impl::fill_rule(d2d_fill_mode mode)
   {
      _mode = mode;
      release(_fill_geom);
   }

   inline void path_impl::close()
   {
      end_path(true);
   }

   inline void path_impl::close_sub_path_if_open()
   {
      if (_path_gens_state == path_started)
         end_path();
   }

   inline d2d_rect* make_rect(rect r)
   {
      d2d_rect* geom = nullptr;
      auto hr = get_factory().CreateRectangleGeometry(
         { r.left, r.top, r.right, r.bottom }, &geom
      );
      if (!SUCCEEDED(hr))
         throw std::runtime_error{ "Error: CreateRectangleGeometry Fail." };
      return geom;
   }

   inline d2d_round_rect* make_round_rect(rect r, float radius)
   {
      d2d_round_rect* geom = nullptr;
      auto hr = get_factory().CreateRoundedRectangleGeometry(
          { { r.left, r.top, r.right, r.bottom }, radius , radius }, &geom
      );
      if (!SUCCEEDED(hr))
         throw std::runtime_error{ "Error: CreateRoundedRectangleGeometry Fail." };
      return geom;
   }

   inline d2d_ellipse* make_circle(circle c)
   {
      d2d_ellipse* geom = nullptr;
      auto hr = get_factory().CreateEllipseGeometry(
         { { c.cx, c.cy }, c.radius, c.radius }, &geom
      );
      if (!SUCCEEDED(hr))
         throw std::runtime_error{ "Error: CreateEllipseGeometry Fail." };
      return geom;
   }

   inline d2d_path* make_path()
   {
      d2d_path* geom = nullptr;
      auto hr = get_factory().CreatePathGeometry(&geom);
      if (!SUCCEEDED(hr))
         throw std::runtime_error{ "Error: CreatePathGeometry Fail." };
      return geom;
   }

   inline d2d_path_sink* start(d2d_path* path)
   {
      d2d_path_sink* sink = nullptr;
      auto hr = path->Open(&sink);
      if (!SUCCEEDED(hr))
         throw std::runtime_error{ "Error: ID2D1PathGeometry::Open Fail." };
      return sink;
   }

   inline void stop(d2d_path_sink* sink)
   {
      auto hr = sink->Close();
      if (!SUCCEEDED(hr))
         throw std::runtime_error{ "Error: ID2D1GeometrySink::Close Fail." };
      release(sink);
   }
}

#endif
