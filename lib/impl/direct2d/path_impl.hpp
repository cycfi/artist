/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#if !defined(ELEMENTS_DETAIL_DIRECT_2D_PATH_IMPL_APR_13_2020)
#define ELEMENTS_DETAIL_DIRECT_2D_PATH_IMPL_APR_13_2020

#include <canvas_impl.hpp>
#include <vector>

namespace cycfi::artist
{
   struct path_impl
   {
   public:

      using geometry_vector = std::vector<d2d_geometry*>;
      using iterator = geometry_vector::iterator;

                           ~path_impl();

      bool                 empty() const;
      void                 add(d2d_geometry* geom);
      void                 clear();
      d2d_geometry*        compute_fill();
      void                 fill_rule(d2d_fill_mode mode);

      iterator             begin()  { return _geometries.begin(); }
      iterator             end()    { return _geometries.end(); }

   private:

      geometry_vector      _geometries;
      d2d_fill_mode        _mode = D2D1_FILL_MODE_WINDING;
      d2d_geometry_group*  _fill_geom = nullptr;
      bool                 _recompute = 0;
   };

   ////////////////////////////////////////////////////////////////////////////
   // Low level utils
   ////////////////////////////////////////////////////////////////////////////
   d2d_rect*         make_rect(rect r);
   d2d_round_rect*   make_round_rect(rect r, float radius);

   ////////////////////////////////////////////////////////////////////////////
   // Inlines
   ////////////////////////////////////////////////////////////////////////////
   inline path_impl::~path_impl()
   {
      for (auto& g : _geometries)
         release(g);
   }

   inline bool path_impl::empty() const
   {
      return _geometries.empty();
   }

   inline void path_impl::add(d2d_geometry* geom)
   {
      _geometries.push_back(geom);
      _recompute = true;
   }

   inline d2d_geometry* path_impl::compute_fill()
   {
      if (_geometries.empty())
         return nullptr;
      if (_geometries.size() == 1)
         return _geometries[0];

      if (_fill_geom)
      {
         if (_recompute)
            release(_fill_geom);
         else
            return _fill_geom;
      }
      _fill_geom = make_group(_geometries, _mode);
      return _fill_geom;
   }

   inline void path_impl::clear()
   {
      for (auto& g : _geometries)
         release(g);
      _geometries.clear();
      _recompute = true;
   }

   inline void path_impl::fill_rule(d2d_fill_mode mode)
   {
      _mode = mode;
      _recompute = true;
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
