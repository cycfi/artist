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

   inline d2d_geometry* path_impl::compute_fill()
   {
      if (_geometries.empty())
         return nullptr;
      if (_geometries.size() == 1)
         return _geometries[0];

      if (_fill_geom)
         return _fill_geom;
      _fill_geom = make_group(_geometries, _mode);
      return _fill_geom;
   }

   inline void path_impl::clear()
   {
      for (auto& g : _geometries)
         release(g);
      _geometries.clear();
      release(_fill_geom);
   }

   inline void path_impl::fill_rule(d2d_fill_mode mode)
   {
      _mode = mode;
      release(_fill_geom);
   }

   inline void path_impl::fill(
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

   inline void path_impl::stroke(
      d2d_canvas& cnv
    , d2d_paint* paint
    , float line_width
    , bool preserve)
   {
      if (!empty())
      {
         for (auto p : _geometries)
            cnv.DrawGeometry(p, paint, line_width, nullptr);
         if (!preserve)
            clear();
      }
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

/*
   struct path_impl
   {
   public:

      using add_path_function = std::function<void(ID2D1GeometrySink*)>;
      using add_path_vector = std::vector<add_path_function>;

                           path_impl();
                           ~path_impl();
                           path_impl(path_impl const& rhs);
                           path_impl(path_impl&& rhs);

      path_impl&           operator=(path_impl const& rhs);
      path_impl&           operator=(path_impl&& rhs);

      void                 add(add_path_function const& add_f);
      ID2D1PathGeometry*   commit();
      ID2D1PathGeometry*   get_geom() const     { return _geom; }
      bool                 is_committed() const { return _committed; }

   private:

      ID2D1PathGeometry*   _geom = nullptr;
      bool                 _committed = false;
      add_path_vector      _add_paths;
   };

   ////////////////////////////////////////////////////////////////////////////
   // Inlines
   ////////////////////////////////////////////////////////////////////////////
   inline path_impl::path_impl()
   {
      auto hr = get_factory().CreatePathGeometry(&_geom);
      if (!SUCCEEDED(hr))
         throw std::runtime_error{ "Error: Creating path." };
   }

   inline path_impl::~path_impl()
   {
      release(_geom);
   }

   inline path_impl::path_impl(path_impl const& rhs)
    : path_impl()
   {
      _add_paths = rhs._add_paths;
   }

   inline path_impl::path_impl(path_impl&& rhs)
    : _geom{ rhs._geom }
    , _committed{ rhs._committed }
    , _add_paths{ std::move(rhs._add_paths) }
   {
      rhs._geom = nullptr;
   }

   inline path_impl& path_impl::operator=(path_impl const& rhs)
   {
      if (this != &rhs)
      {
         if (_committed)
         {
            ID2D1PathGeometry* new_geom = nullptr;
            auto hr = get_factory().CreatePathGeometry(&new_geom);
            if (!SUCCEEDED(hr))
               throw std::runtime_error{ "Error: Copying path." };
            release(_geom);
            _geom = new_geom;
            _committed = false;
         }
         _add_paths = rhs._add_paths;
      }
      return *this;
   }

   inline path_impl& path_impl::operator=(path_impl&& rhs)
   {
      if (this != &rhs)
      {
         _geom = rhs._geom; rhs._geom = nullptr;
         _committed = rhs._committed;
         _add_paths = std::move(rhs._add_paths);
      }
      return *this;
   }

   inline void path_impl::add(add_path_function const& add_f)
   {
      _add_paths.push_back(add_f);
   }

   inline ID2D1PathGeometry* path_impl::commit()
   {
      if (_committed)
      {
         release(_geom);
         auto hr = get_factory().CreatePathGeometry(&_geom);
         if (!SUCCEEDED(hr))
            throw std::runtime_error{ "Error: Committing path." };
      }

      ID2D1GeometrySink* psink = nullptr;
      auto hr = _geom->Open(&psink);
      if (!SUCCEEDED(hr))
         throw std::runtime_error{ "Error: Committing path." };

      for (auto const& add_path : _add_paths)
         add_path(psink);
      _committed = true;
      psink->Close();
      release(psink);
      return _geom;
   }
*/
}

#endif
