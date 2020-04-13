/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#if !defined(ELEMENTS_DETAIL_DIRECT_2D_PATH_IMPL_APR_13_2020)
#define ELEMENTS_DETAIL_DIRECT_2D_PATH_IMPL_APR_13_2020

#include <canvas_impl.hpp>
#include <functional>
#include <vector>

namespace cycfi::artist
{
   struct path_impl
   {
      std::vector<ID2D1Geometry*> _geometries;
   };








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
