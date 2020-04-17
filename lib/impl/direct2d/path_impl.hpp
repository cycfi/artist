/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#if !defined(ARTIST_D2D_PATH_IMPL_APR_13_2020)
#define ARTIST_D2D_PATH_IMPL_APR_13_2020

#include <context.hpp>
#include <vector>
#include <variant>

namespace cycfi::artist::d2d
{
   struct path_impl
   {
   public:

      enum render_mode
      {
         fill_winding = fill_mode_winding,
         fill_odd_even = fill_mode_alternate,
         stroke_mode = -1
      };

      using path_gen = std::function<void(geometry_sink* sink, render_mode mode)>;
      using path_gen_vector = std::vector<path_gen>;

      using geometry_gen = std::function<geometry*(render_mode mode)>;
      using geometry_gen_vector = std::vector<geometry_gen>;

      using geometry_vector = std::vector<geometry*>;
      using iterator = geometry_vector::iterator;

                           ~path_impl();

      bool                 empty() const;
      void                 clear();
      void                 fill_rule(fill_mode mode);

      void                 add(rect r);
      void                 add(rect r, float radius);
      void                 add(circle c);

      void                 fill(
                              render_target& target
                            , brush* paint
                            , bool preserve
                           );

      void                 stroke(
                              render_target& target
                            , brush* paint
                            , float line_width
                            , bool preserve
                            , stroke_style* stroke_style
                           );

      rect                 fill_bounds(render_target& target);
      rect                 stroke_bounds(
                              render_target& target
                            , float line_width
                            , stroke_style* stroke_style
                           );

      void                 begin_path();
      void                 end_path(bool close = false);
      void                 close_path();
      void                 move_to(point p);
      void                 line_to(point p);
      void                 arc(
                              point p, float radius
                            , float start_angle, float end_angle
                            , bool ccw
                           );
      void                 arc_to(point p1, point p2, float radius);

      void                 quadratic_curve_to(point cp, point end);
      void                 bezier_curve_to(point cp1, point cp2, point end);

   private:

      enum path_gen_state { path_started, path_ended };

      void                 add_gen(geometry_gen&& gen);
      void                 close_sub_path_if_open();
      void                 build_path();
      void                 compute_geometries(render_mode mode);
      void                 clear_geometries();
      geometry*            compute_fill();

      geometry_vector      _geometries;
      geometry_gen_vector  _geometry_gens;
      fill_mode            _mode = fill_mode_winding;
      geometry_group*      _fill_geometry = nullptr;
      path_gen_vector      _path_gens;
      path_gen_state       _path_gens_state = path_ended;
      point                _start;
      point                _cp;
      int                  _id = -1;
      int                  _generation = 0;
   };

   ////////////////////////////////////////////////////////////////////////////
   // Low level utils
   ////////////////////////////////////////////////////////////////////////////
   rect_geometry*          make_rect(rect r);
   round_rect_geometry*    make_round_rect(rect r, float radius);
   ellipse_geometry*       make_circle(circle c);
   stroke_style*           make_stroke_style(
                              canvas::line_cap_enum line_cap
                            , canvas::join_enum join
                            , float miter_limit
                           );

   path_geometry*          make_path();
   geometry_sink*          start(path_geometry* path);
   void                    stop(geometry_sink* sink);

   ////////////////////////////////////////////////////////////////////////////
   // Inlines
   ////////////////////////////////////////////////////////////////////////////
   inline path_impl::~path_impl()
   {
      for (auto& g : _geometries)
         release(g);
      release(_fill_geometry);
   }

   inline bool path_impl::empty() const
   {
      return _geometry_gens.empty();
   }

   inline void path_impl::add_gen(geometry_gen&& gen)
   {
      _geometry_gens.emplace_back(std::move(gen));
      release(_fill_geometry);
      ++_id;
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

   inline void path_impl::fill_rule(fill_mode mode)
   {
      _mode = mode;
      release(_fill_geometry);
   }

   inline void path_impl::close_path()
   {
      end_path(true);
   }

   inline void path_impl::close_sub_path_if_open()
   {
      if (_path_gens_state == path_started)
         end_path();
   }

   inline rect_geometry* make_rect(rect r)
   {
      rect_geometry* geom = nullptr;
      auto hr = get_factory().CreateRectangleGeometry(
         { r.left, r.top, r.right, r.bottom }, &geom
      );
      if (!SUCCEEDED(hr))
         throw std::runtime_error{ "Error: CreateRectangleGeometry Fail." };
      return geom;
   }

   inline round_rect_geometry* make_round_rect(rect r, float radius)
   {
      round_rect_geometry* geom = nullptr;
      auto hr = get_factory().CreateRoundedRectangleGeometry(
          { { r.left, r.top, r.right, r.bottom }, radius , radius }, &geom
      );
      if (!SUCCEEDED(hr))
         throw std::runtime_error{ "Error: CreateRoundedRectangleGeometry Fail." };
      return geom;
   }

   inline ellipse_geometry* make_circle(circle c)
   {
      ellipse_geometry* geom = nullptr;
      auto hr = get_factory().CreateEllipseGeometry(
         { { c.cx, c.cy }, c.radius, c.radius }, &geom
      );
      if (!SUCCEEDED(hr))
         throw std::runtime_error{ "Error: CreateEllipseGeometry Fail." };
      return geom;
   }

   inline stroke_style* make_stroke_style(
      canvas::line_cap_enum line_cap_
    , canvas::join_enum join_
    , float miter_limit
   )
   {
      cap_style line_cap = cap_style_flat;
      line_join join = line_join_miter;

      switch (line_cap_)
      {
         case canvas::butt: line_cap = cap_style_flat; break;
         case canvas::round: line_cap = cap_style_round; break;
         case canvas::square: line_cap = cap_style_square; break;
      };

      switch (join_)
      {
         case canvas::bevel_join: join = line_join_bevel; break;
         case canvas::round_join: join = line_join_round; break;
         case canvas::miter_join: join = line_join_miter; break;
      };

      stroke_style* style = nullptr;
      auto hr = get_factory().CreateStrokeStyle(
              stroke_style_properties{
            line_cap,
            line_cap,
            line_cap,
            join,
            miter_limit,
            D2D1_DASH_STYLE_SOLID,
            0.0f
         }
       , nullptr, 0, &style
      );
      if (!SUCCEEDED(hr))
         throw std::runtime_error{ "Error: CreateStrokeStyle Fail." };
      return style;
   }

   inline path_geometry* make_path()
   {
      path_geometry* geom = nullptr;
      auto hr = get_factory().CreatePathGeometry(&geom);
      if (!SUCCEEDED(hr))
         throw std::runtime_error{ "Error: CreatePathGeometry Fail." };
      return geom;
   }

   inline geometry_sink* start(path_geometry* path)
   {
      geometry_sink* sink = nullptr;
      auto hr = path->Open(&sink);
      if (!SUCCEEDED(hr))
         throw std::runtime_error{ "Error: ID2D1PathGeometry::Open Fail." };
      return sink;
   }

   inline void stop(geometry_sink* sink)
   {
      auto hr = sink->Close();
      if (!SUCCEEDED(hr))
         throw std::runtime_error{ "Error: ID2D1GeometrySink::Close Fail." };
      release(sink);
   }
}

#endif
