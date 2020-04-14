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
   struct pe_round_rect { rect r; float radius; };

   using path_element = std::variant<
      rect
    , pe_round_rect
    , circle
   >;

   struct path_impl
   {
   public:

      using element_vector = std::vector<path_element>;
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
                            , bool preserve);

      void                 stroke(
                              d2d_canvas& cnv
                            , d2d_paint* paint
                            , float line_width
                            , bool preserve);

   private:

      void                 add(d2d_geometry* geom);

      element_vector       _elements;
      geometry_vector      _geometries;
      d2d_fill_mode        _mode = D2D1_FILL_MODE_WINDING;
      d2d_geometry_group*  _fill_geom = nullptr;
   };

   ////////////////////////////////////////////////////////////////////////////
   // Low level utils
   ////////////////////////////////////////////////////////////////////////////
   d2d_rect*               make_rect(rect r);
   d2d_round_rect*         make_round_rect(rect r, float radius);
   d2d_ellipse*            make_circle(circle c);
   d2d_path*               make_path();
   d2d_path_builder*       start(d2d_path* path);
   void                    stop(d2d_path_builder* builder);

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
      return _geometries.empty();
   }

   inline void path_impl::add(d2d_geometry* geom)
   {
      _geometries.push_back(geom);
      release(_fill_geom);
   }

   inline void path_impl::add(rect r)
   {
      _elements.push_back(r);
      add(make_rect(r));
   }

   inline void path_impl::add(rect r, float radius)
   {
      _elements.push_back(pe_round_rect{ r, radius });
      add(make_round_rect(r, radius));
   }

   inline void path_impl::add(circle c)
   {
      _elements.push_back(c);
      add(make_circle(c));
   }

   inline void path_impl::fill_rule(d2d_fill_mode mode)
   {
      _mode = mode;
      release(_fill_geom);
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

   inline d2d_path_builder* start(d2d_path* path)
   {
      d2d_path_builder* builder = nullptr;
      auto hr = path->Open(&builder);
      if (!SUCCEEDED(hr))
         throw std::runtime_error{ "Error: ID2D1PathGeometry::Open Fail." };
      return builder;
   }

   inline void stop(d2d_path_builder* builder)
   {
      auto hr = builder->Close();
      if (!SUCCEEDED(hr))
         throw std::runtime_error{ "Error: ID2D1GeometrySink::Close Fail." };
   }
}

#endif
