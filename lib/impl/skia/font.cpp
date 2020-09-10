/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <artist/font.hpp>
#include <SkTypeface.h>
#include <SkFont.h>
#include <sstream>
#include <SkFontMetrics.h>
#include <SkFontMgr.h>
#include <infra/filesystem.hpp>
#include <infra/support.hpp>

# include <fontconfig/fontconfig.h>
# include <map>
# include <mutex>

# if defined(_WIN32)
# include <Windows.h>
# include "sysinfoapi.h"
# include "tchar.h"
# include <SkTypeface_win.h>
# endif

namespace cycfi::artist
{
   namespace
   {
      inline void ltrim(std::string& s)
      {
         s.erase(s.begin(), std::find_if(s.begin(), s.end(),
            [](int ch) { return ch != ' ' && ch != '"'; }
         ));
      }

      inline void rtrim(std::string& s)
      {
         s.erase(std::find_if(s.rbegin(), s.rend(),
            [](int ch) { return ch != ' ' && ch != '"'; }
         ).base(), s.end());
      }

      inline void trim(std::string& s)
      {
         ltrim(s);
         rtrim(s);
      }
   }

   namespace
   {
      inline float lerp(float a, float b, float f)
      {
         return (a * (1.0 - f)) + (b * f);
      }

      using skia_font_map_type = std::map<std::string, sk_sp<SkTypeface>>;

      std::pair<skia_font_map_type&, std::mutex&> get_skia_font_map()
      {
         static skia_font_map_type map_;
         static std::mutex mutex_;
         return { map_, mutex_ };
      }

      struct font_entry
      {
         std::string    full_name;
         std::string    file;
         int            index    = 0;
         uint8_t        weight   = font_constants::weight_normal;
         uint8_t        slant    = font_constants::slant_normal;
         uint8_t        stretch  = font_constants::stretch_normal;
      };

      using font_map_type = std::map<std::string, std::vector<font_entry>>;
      font_map_type& font_map()
      {
         static font_map_type font_map_;
         return font_map_;
      }

      enum
      {
         fc_thin            = 0,
         fc_extralight      = 40,
         fc_light           = 50,
         fc_semilight       = 55,
         fc_book            = 75,
         fc_normal          = 80,
         fc_medium          = 100,
         fc_semibold        = 180,
         fc_bold            = 200,
         fc_extrabold       = 205,
         fc_black           = 210
      };

      int map_fc_weight(int w)
      {
         auto&& map = [](double mina, double maxa, double minb, double maxb, double val)
         {
            return lerp(mina, maxa, (val-minb)/(maxb-minb));
         };

         namespace fc = font_constants;

         if (w < fc_extralight)
            return map(fc::thin, fc::extra_light, fc_thin, fc_extralight, w);
         if (w < fc_light)
            return map(fc::extra_light, fc::light, fc_extralight, fc_light, w);
         if (w < fc_normal)
            return map(fc::light, fc::weight_normal, fc_light, fc_normal, w);
         if (w < fc_medium)
            return map(fc::weight_normal, fc::medium, fc_normal, fc_medium, w);
         if (w < fc_semibold)
            return map(fc::medium, fc::semi_bold, fc_medium, fc_semibold, w);
         if (w < fc_bold)
            return map(fc::semi_bold, fc::bold, fc_semibold, fc_bold, w);
         if (w < fc_extrabold)
            return map(fc::bold, fc::extra_bold, fc_bold, fc_extrabold, w);
         if (w < fc_black)
            return map(fc::extra_bold, fc::black, fc_extrabold, fc_black, w);
         return map(fc::black, 100, fc_black, 220, std::min(w, 220));
      }

      using fc_config_ptr = std::unique_ptr<FcConfig, deleter<FcConfig, FcConfigDestroy>>;
      using fc_patern_ptr = std::unique_ptr<FcPattern, deleter<FcPattern, FcPatternDestroy>>;
      using fc_object_set_ptr = std::unique_ptr<FcObjectSet, deleter<FcObjectSet, FcObjectSetDestroy>>;
      using fc_font_set_ptr = std::unique_ptr<FcFontSet, deleter<FcFontSet, FcFontSetDestroy>>;

      void init_font_map()
      {
         FcInit();
         auto config = fc_config_ptr{ FcConfigCreate() };

#if defined(__APPLE__)
         auto app_fonts_path = get_user_fonts_directory();
#else
         auto app_fonts_path = fs::current_path() / "resources";
#endif
         FcConfigAppFontAddDir(config.get(), (FcChar8 const*)app_fonts_path.string().c_str());
         auto pat = fc_patern_ptr{ FcPatternCreate() };
         auto os = fc_object_set_ptr{
                     FcObjectSetBuild(
                        FC_FAMILY, FC_FULLNAME, FC_WIDTH, FC_WEIGHT
                      , FC_SLANT, FC_FILE, FC_INDEX, nullptr)
                     };
         auto fs = fc_font_set_ptr{ FcFontList(config.get(), pat.get(), os.get()) };

         for (int i=0; fs && i < fs->nfont; ++i)
         {
            FcPattern* font = fs->fonts[i];
            FcChar8 *file, *family, *full_name;
            int index;
            if (FcPatternGetString(font, FC_FILE, 0, &file) == FcResultMatch &&
               FcPatternGetString(font, FC_FAMILY, 0, &family) == FcResultMatch &&
               FcPatternGetString(font, FC_FULLNAME, 0, &full_name) == FcResultMatch &&
               FcPatternGetInteger(font, FC_INDEX, 0, &index) == FcResultMatch
            )
            {
               font_entry entry;
               entry.full_name = (const char*) full_name;
               entry.file = (const char*) file;
               entry.index = index;

               int weight;
               if (FcPatternGetInteger(font, FC_WEIGHT, 0, &weight) == FcResultMatch)
                  entry.weight = map_fc_weight(weight); // map the weight (normalized 0 to 100)

               int slant;
               if (FcPatternGetInteger(font, FC_SLANT, 0, &slant) == FcResultMatch)
                  entry.slant = (slant * 100) / 110; // normalize 0 to 100

               int width;
               if (FcPatternGetInteger(font, FC_WIDTH, 0, &width) == FcResultMatch)
                  entry.stretch = (width * 100) / 200; // normalize 0 to 100

               std::string key = (const char*) family;
               trim(key);

               if (auto it = font_map().find(key); it != font_map().end())
               {
                  it->second.push_back(entry);
               }
               else
               {
                  font_map()[key] = {};
                  font_map()[key].push_back(entry);
               }
            }
         }
      }


      font_entry const* match(font_descr descr)
      {
         struct font_init
         {
            font_init()
            {
               init_font_map();
            }
         };
         static font_init init;

         std::istringstream str(std::string{ descr._families });
         std::string family;
         while (getline(str, family, ','))
         {
            trim(family);
            if (auto i = font_map().find(family); i != font_map().end())
            {
               int min = 10000;
               std::vector<font_entry>::const_iterator best_match = i->second.end();
               for (auto j = i->second.begin(); j != i->second.end(); ++j)
               {
                  auto const& item = *j;

                  // Get biased score (lower is better). Give `slant` attribute
                  // the highest bias (3.0), followed by `weight` (1.0) and then
                  // `stretch` (0.25).
                  auto diff =
                     (std::abs(int(descr._weight) - int(item.weight)) * 1.0) +
                     (std::abs(int(descr._slant) - int(item.slant)) * 3.0) +
                     (std::abs(int(descr._stretch) - int(item.stretch)) * 0.25)
                     ;
                  if (diff < min)
                  {
                     min = diff;
                     best_match = j;
                     if (diff == 0)
                        break;
                  }
               }
               if (best_match != i->second.end())
                  return &*best_match;
            }
         }
         return nullptr;
      }
   }

   font::font()
    : _ptr(std::make_shared<SkFont>())
   {
   }

   font::font(font_descr descr)
   {
      auto match_ptr = match(descr);
      if (match_ptr)
      {
         auto [skia_font_map, skia_font_map_mutex] = get_skia_font_map();
         std::lock_guard<std::mutex> lock(skia_font_map_mutex);
         if (auto it = skia_font_map.find(match_ptr->full_name); it != skia_font_map.end())
         {
            _ptr = std::make_shared<SkFont>(it->second, descr._size);
         }
         else
         {
            auto face = SkTypeface::MakeFromFile(match_ptr->file.c_str(), match_ptr->index);
            _ptr = std::make_shared<SkFont>(face, descr._size);
            if (_ptr)
               skia_font_map[match_ptr->full_name] = face;
         }
      }

      if (_ptr)
         return;

      using namespace font_constants;
      int stretch = int(descr._stretch) / 10;
      SkFontStyle style(
         descr._weight * 10
       , (descr._stretch < condensed)? stretch-1 : stretch
       , (descr._slant == italic)? SkFontStyle::kItalic_Slant :
         (descr._slant == oblique)? SkFontStyle::kOblique_Slant :
         SkFontStyle::kUpright_Slant
      );

      auto default_face = SkTypeface::MakeFromName(nullptr, style);
      std::istringstream str(std::string{ descr._families });
      std::string family;

      while (getline(str, family, ','))
      {
         trim(family);
         auto face = SkTypeface::MakeFromName(family.c_str(), style);
         if (face && face != default_face)
         {
            _ptr = std::make_shared<SkFont>(face, descr._size);
            break;
         }
      }
      if (!_ptr)
         _ptr = std::make_shared<SkFont>(default_face, descr._size);
   }

   font::font(font const& rhs)
   {
      _ptr = rhs._ptr;
   }

   font::font(font&& rhs) noexcept
    : _ptr(std::move(rhs._ptr))
   {
   }

   font::~font()
   {
   }

   font& font::operator=(font const& rhs)
   {
      _ptr = rhs._ptr;
      return *this;
   }

   font& font::operator=(font&& rhs) noexcept
   {
      _ptr = std::move(rhs._ptr);
      return *this;
   }

   font::metrics_info font::metrics() const
   {
      SkFontMetrics sk_metrics;
      _ptr->getMetrics(&sk_metrics);
      return { -sk_metrics.fAscent, sk_metrics.fDescent, sk_metrics.fLeading };
   }

   float font::measure_text(std::string_view str) const
   {
      return _ptr->measureText(str.data(), str.size(), SkTextEncoding::kUTF8);
   }
}


