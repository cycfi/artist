/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <artist/font.hpp>
#include <sstream>
#include <infra/filesystem.hpp>
#include <infra/support.hpp>

#include <cairo.h>
#include <cairo-ft.h>

# include <fontconfig/fontconfig.h>
# include <map>
# include <mutex>

#include "font_impl.h"

#ifdef __APPLE__
# include <cairo-quartz.h>
#endif

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

   namespace // FONT CONFIG -- possibly move to impl/fontconfig
   {
      inline float lerp(float a, float b, float f)
      {
         return (a * (1.0 - f)) + (b * f);
      }

      struct font_entry
      {
         //sk_sp<SkTypeface> cached_typeface;
         cairo_font_face_t* cached_typeface;
         std::string       file;
         std::string       full_name;
         int               index    = 0;
         uint8_t           weight   = font_constants::weight_normal;
         uint8_t           slant    = font_constants::slant_normal;
         uint8_t           stretch  = font_constants::stretch_normal;
      };

      using font_map_type = std::map<std::string, std::vector<font_entry>>;
      std::pair<font_map_type&, std::mutex&> get_font_map()
      {
         static font_map_type font_map_;
         static std::mutex mutex_;
         return {font_map_, mutex_};
      }

      constexpr auto font_map_default_font_family = "";

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
         FcConfig* config = FcConfigGetCurrent();
         auto user_fonts_path = get_user_fonts_directory();
         FcConfigAppFontAddDir(config, (FcChar8 const*)user_fonts_path.string().c_str());
         auto pat = fc_patern_ptr{FcPatternCreate()};
         auto os = fc_object_set_ptr{
                     FcObjectSetBuild(
                        FC_FAMILY, FC_FULLNAME, FC_WIDTH, FC_WEIGHT
                      , FC_SLANT, FC_FILE, FC_INDEX, nullptr)
                     };
         auto fs = fc_font_set_ptr{FcFontList(config, pat.get(), os.get())};

         // The lock is not needed, since this is run on static initialization.
         auto [font_map, font_map_mutex] = get_font_map();

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
               entry.cached_typeface = nullptr;
               entry.file = (const char*) file;
               entry.full_name = (const char*) full_name;
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

               font_map[key].push_back(std::move(entry));
            }
         }
      }

      font_entry* match(font_map_type& font_map, font_descr descr)
      {
         struct font_init
         {
            font_init()
            {
               init_font_map();
            }
         };
         static font_init init;

         std::istringstream str(
            std::string{descr._families} + ", " + font_map_default_font_family);
         std::string family;
         while (getline(str, family, ','))
         {
            trim(family);
            if (auto i = font_map.find(family); i != font_map.end())
            {
               int min = 10000;
               std::vector<font_entry>::iterator best_match = i->second.end();
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
   
   namespace { // FREE_TYPE
      class free_type_face
      {
      public:
         free_type_face() = default;

         free_type_face(FT_Face face)
         : _face(face) {}

         ~free_type_face()
         {
            if (_face)
               FT_Done_Face(_face);
         }

         free_type_face(free_type_face const& other) = delete;
         free_type_face& operator=(free_type_face const& other) = delete;

         free_type_face(free_type_face&& other) noexcept
         : free_type_face()
         {
            *this = std::move(other);
         }

         free_type_face& operator=(free_type_face&& other) noexcept
         {
            std::swap(_face, other._face);
            return *this;
         }

         FT_Face handle()
         {
            return _face;
         }

         FT_Face release()
         {
            return std::exchange(_face, nullptr);
         }

         explicit operator bool() const
         {
            return _face != nullptr;
         }

      private:
         FT_Face _face = nullptr;
      };

      void destroy_free_type_face(void* face)
      {
         FT_Done_Face(reinterpret_cast<FT_Face>(face));
      }

      class free_type_library
      {
      public:
         free_type_library()
         {
            FT_Error status = FT_Init_FreeType(&_ft_lib);
            //CYCFI_ASSERT(status == 0, "Error: failed to initialize free type library.");
         }

         ~free_type_library()
         {
            if (!_ft_lib)
               return;

            FT_Error status = FT_Done_FreeType(_ft_lib);
            //CYCFI_ASSERT(status == 0, "Error: failed to destroy free type library.");
         }

         free_type_library(free_type_library const& other) = delete;
         free_type_library& operator=(free_type_library const& other) = delete;

         free_type_library(free_type_library&& other) noexcept
         {
            swap(*this, other);
         }

         free_type_library& operator=(free_type_library&& other) noexcept
         {
            swap(*this, other);
            return *this;
         }

         friend void swap(free_type_library& lhs, free_type_library& rhs) noexcept
         {
            std::swap(lhs._ft_lib, rhs._ft_lib);
         }

         free_type_face load_face(char const* font_path)
         {
            FT_Face ft_face;
            FT_Error ft_status = FT_New_Face(_ft_lib, font_path, 0, &ft_face);

            if (ft_status == 0)
               return free_type_face(ft_face);
            else
               return free_type_face(nullptr);
         }

      private:
         FT_Library _ft_lib = nullptr;
      };
   }

    auto const& cairo_user_data_key()
    {
      static const cairo_user_data_key_t key = {};
      return key;
    }
    
    cairo_font_face_t* load_free_type_font(char const* font_path)
    {
      static free_type_library ft_lib;
    
      free_type_face ft_face = ft_lib.load_face(font_path);
      if (!ft_face)
        return nullptr;

      cairo_font_face_t* cairo_face = cairo_ft_font_face_create_for_ft_face(ft_face.handle(), 0);
      if (cairo_face == nullptr)
        return nullptr;

      // extend the freetype font face lifetime to cairo's font face lifetime
      cairo_status_t cairo_status = CAIRO_STATUS_SUCCESS;
      cairo_user_data_key_t const& key = cairo_user_data_key();
      if (cairo_font_face_get_user_data(cairo_face, &key) != ft_face.handle())
      {
        cairo_status = cairo_font_face_set_user_data(
            cairo_face, &key, ft_face.handle(), &destroy_free_type_face);
      }

      if (cairo_status == CAIRO_STATUS_SUCCESS)
      {
        ft_face.release();
      }
      else
      {
        cairo_font_face_destroy(cairo_face);
        return nullptr;
      }

      return cairo_face;
    }
    
    cairo_font_face_t* load_apple_font(const char* name) {
      auto cfstr = CFStringCreateWithCString(
        kCFAllocatorDefault
        , name
        , kCFStringEncodingUTF8
      );
      auto cgfont = CGFontCreateWithFontName(cfstr);
      auto cairo_face = cairo_quartz_font_face_create_for_cgfont(cgfont);
      if (cgfont)
        CFRelease(cgfont);
      if (cfstr)
        CFRelease(cfstr);
      return cairo_face;
    }

   font::font()
    : _ptr(std::make_shared<font_impl>())
   {
   }

   font::font(font_descr descr)
   {
      auto [font_map, font_map_mutex] = get_font_map();
      std::lock_guard<std::mutex> lock(font_map_mutex);

      auto match_ptr = match(font_map, descr);
      if (match_ptr)
      {
         if( !match_ptr->cached_typeface ) {
#ifdef __APPLE__
             match_ptr->cached_typeface = load_apple_font(match_ptr->full_name.c_str());
#else
             match_ptr->cached_typeface = load_free_type_font(match_ptr->file.c_str());
#endif
         }
      
         if (match_ptr->cached_typeface)
         {
            _ptr = std::make_shared<font_impl>(cairo_font_face_reference(match_ptr->cached_typeface), descr._size);
         }
      }

      if (_ptr)
         return;
/*
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
      std::istringstream str(std::string{descr._families});
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
      {
         family = font_map_default_font_family;
         _ptr = std::make_shared<SkFont>(default_face, descr._size);
      }

      font_entry entry;
      entry.cached_typeface = sk_ref_sp(_ptr->getTypeface());
      entry.weight = descr._weight;
      entry.slant = descr._slant;
      entry.stretch = descr._stretch;

      font_map[family].push_back(std::move(entry));
*/
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
      // Create a temp cairo context, set font and get extents?
      return {0,0,0};
   }

   float font::measure_text(std::string_view str) const
   {
      // not sure what the intent of this measure is
      // but seems like it also needs a temp cairo context
      return 0;
   }
}


