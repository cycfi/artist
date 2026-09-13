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
#include <unordered_map>

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
   // Text measurements already taken, keyed by the font's shared identity
   // (what copies of a font share underneath) and the text's hash. A hit is
   // confirmed against the stored text, so a hash collision measures again.
   // Each entry keeps a copy of its font, so the identity cannot be reused by
   // another font while the entry lives. The cache empties itself when it
   // grows past max_size.
   //
   // get_measure_cache never destroys its cache, for the same reason as
   // get_font_cache.
   ////////////////////////////////////////////////////////////////////////////
   template <typename Font, typename Metrics>
   class measure_cache
   {
   public:

      static constexpr std::size_t max_size = 4096;

      template <typename Measure>
      Metrics              get(
                              void const* id, Font const& font_
                            , std::string_view text, Measure&& measure
                           );

   private:

      struct entry
      {
         void const*       id;
         std::string       text;
         Metrics           metrics;
         Font              font;
      };

      using map_type = std::unordered_map<std::size_t, entry>;

      std::mutex           _mutex;
      map_type             _map;
   };

   template <typename Font, typename Metrics>
   measure_cache<Font, Metrics>& get_measure_cache();

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

   template <typename Font, typename Metrics>
   template <typename Measure>
   inline Metrics measure_cache<Font, Metrics>::get(
      void const* id, Font const& font_
    , std::string_view text, Measure&& measure
   )
   {
      auto h = std::hash<std::string_view>{}(text);
      h ^= std::hash<void const*>{}(id) + 0x9e3779b9 + (h << 6) + (h >> 2);

      std::lock_guard<std::mutex> lock(_mutex);
      auto i = _map.find(h);
      if (i != _map.end() && i->second.id == id && i->second.text == text)
         return i->second.metrics;

      Metrics m = measure();
      if (i != _map.end())
      {
         i->second = entry{id, std::string{text}, m, font_};
      }
      else
      {
         if (_map.size() >= max_size)
            _map.clear();
         _map.emplace(h, entry{id, std::string{text}, m, font_});
      }
      return m;
   }

   template <typename Font, typename Metrics>
   inline measure_cache<Font, Metrics>& get_measure_cache()
   {
      static auto* cache = new measure_cache<Font, Metrics>;
      return *cache;
   }
}

#endif
