/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#if !defined(ARTIST_DETAIL_FONT_CACHE_SEPTEMBER_13_2026)
#define ARTIST_DETAIL_FONT_CACHE_SEPTEMBER_13_2026

#include <artist/font.hpp>
#include <cstddef>
#include <functional>
#include <map>
#include <mutex>
#include <string>
#include <string_view>
#include <tuple>

namespace cycfi::artist::detail
{
   ////////////////////////////////////////////////////////////////////////////
   // Fonts already built, keyed by their full description, so that building
   // a font from a description seen before is a lookup and a copy. Fonts do
   // not change once built, so a copy is as good as a new build. The cache
   // empties itself when it grows past max_size.
   //
   // get_font_cache never destroys its cache: cached fonts hold backend
   // resources that may already be gone during static destruction.
   ////////////////////////////////////////////////////////////////////////////
   template <typename Font>
   class font_cache
   {
   public:

      static constexpr std::size_t max_size = 256;

      template <typename Build>
      Font                 get(font_descr const& descr, Build&& build);

   private:

      using key_type = std::tuple<std::string, float, int, int, int>;
      using map_type = std::map<key_type, Font, std::less<>>;

      std::mutex           _mutex;
      map_type             _map;
   };

   template <typename Font>
   font_cache<Font>&       get_font_cache();

   ////////////////////////////////////////////////////////////////////////////
   // Inlines
   ////////////////////////////////////////////////////////////////////////////
   template <typename Font>
   template <typename Build>
   inline Font font_cache<Font>::get(font_descr const& descr, Build&& build)
   {
      auto const key = std::make_tuple(
         descr._families, descr._size
       , int(descr._weight), int(descr._slant), int(descr._stretch)
      );

      std::lock_guard<std::mutex> lock(_mutex);
      if (auto i = _map.find(key); i != _map.end())
         return i->second;

      if (_map.size() >= max_size)
         _map.clear();

      Font f = build(descr);
      _map.emplace(
         key_type{
            std::string{descr._families}, descr._size
          , int(descr._weight), int(descr._slant), int(descr._stretch)
         }
       , f
      );
      return f;
   }

   template <typename Font>
   inline font_cache<Font>& get_font_cache()
   {
      static auto* cache = new font_cache<Font>;
      return *cache;
   }
}

#endif
