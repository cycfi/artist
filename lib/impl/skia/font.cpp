/*=============================================================================
   Copyright (c) 2016-2023 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <artist/font.hpp>
#include <SkTypeface.h>
#include <SkFont.h>
#include <sstream>
#include <SkFontMetrics.h>
#include <SkFontMgr.h>
#include <SkFontTypes.h>
#include <infra/filesystem.hpp>
#include <infra/support.hpp>

#if !defined(__APPLE__) && !defined(_WIN32)
# include <fontconfig/fontconfig.h>
#endif
#include <map>
#include <mutex>

#if defined(__APPLE__)
# include <ports/SkFontMgr_mac_ct.h>
#elif defined(_WIN32)
# include <Windows.h>
# include "sysinfoapi.h"
# include "tchar.h"
# include <ports/SkTypeface_win.h>   // declares SkFontMgr_New_DirectWrite()
#else
# include <ports/SkFontMgr_fontconfig.h>
# include <ports/SkFontScanner_FreeType.h>
#endif

namespace cycfi::artist
{
   namespace
   {
      sk_sp<SkFontMgr> get_font_mgr()
      {
#if defined(__APPLE__)
         return SkFontMgr_New_CoreText(nullptr);
#elif defined(_WIN32)
         return SkFontMgr_New_DirectWrite();
#else
         // m148: SkFontMgr_New_FontConfig now requires an explicit font
         // scanner. Use the FreeType scanner (matches SK_TYPEFACE_FACTORY_FREETYPE).
         return SkFontMgr_New_FontConfig(nullptr, SkFontScanner_Make_FreeType());
#endif
      }
   }

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

      struct font_entry
      {
         sk_sp<SkTypeface> cached_typeface;
         std::string       file;
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

#if !defined(__APPLE__) && !defined(_WIN32)
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
#endif

#if defined(__APPLE__) || defined(_WIN32)
      // Read an OpenType 'name' table entry (by name ID) from a typeface,
      // returned as ASCII. Prefers the Windows (3,1,0x409) record, falling
      // back to the Mac (1,0) record. Used to recover the typographic family
      // name (ID 16): DirectWrite's getFamilyName() returns the legacy ID 1
      // family (e.g. "Open Sans Condensed Light") rather than the typographic
      // family ("Open Sans Condensed") that callers request. Empty if absent.
      std::string get_name_id(SkTypeface* tf, unsigned want_id)
      {
         constexpr SkFontTableTag name_tag = SkSetFourByteTag('n', 'a', 'm', 'e');
         size_t size = tf->getTableSize(name_tag);
         if (size < 6)
            return {};
         std::vector<uint8_t> buf(size);
         if (tf->getTableData(name_tag, 0, size, buf.data()) != size)
            return {};

         auto u16 = [&](size_t o) -> unsigned {
            return (o + 1 < buf.size())? (unsigned(buf[o]) << 8 | buf[o + 1]) : 0u;
         };
         unsigned count = u16(2);
         unsigned storage = u16(4);
         std::string win, mac;
         for (unsigned i = 0; i < count; ++i)
         {
            size_t rec = 6 + i * 12;
            if (rec + 12 > size)
               break;
            unsigned platform = u16(rec);
            unsigned encoding = u16(rec + 2);
            unsigned language = u16(rec + 4);
            unsigned nameid   = u16(rec + 6);
            unsigned len      = u16(rec + 8);
            unsigned off      = u16(rec + 10);
            if (nameid != want_id)
               continue;
            size_t str = storage + off;
            if (str + len > size)
               continue;
            if (platform == 3 && encoding == 1) // Windows, UTF-16BE
            {
               std::string s;
               for (unsigned k = 0; k + 1 < len; k += 2)
               {
                  unsigned cp = unsigned(buf[str + k]) << 8 | buf[str + k + 1];
                  s.push_back(cp < 0x80? char(cp) : '?');
               }
               if (language == 0x409 || win.empty())
                  win = s;
            }
            else if (platform == 1 && encoding == 0) // Mac Roman (ASCII subset)
            {
               mac.assign(reinterpret_cast<char const*>(&buf[str]), len);
            }
         }
         return !win.empty()? win : mac;
      }
#endif

      void init_font_map()
      {
#if defined(__APPLE__) || defined(_WIN32)
         // On macOS and Windows, fontconfig is unavailable. Scan the user fonts
         // directory using Skia (portable across the CoreText and DirectWrite
         // font managers) so we use the same bundled font files as other
         // backends. Without this, only system-installed fonts are visible and
         // the bundled fonts (used by tests/examples) load as null typefaces,
         // yielding zero metrics and downstream crashes.
         auto mgr = get_font_mgr();
         auto user_fonts_path = get_user_fonts_directory();
         auto [font_map, font_map_mutex] = get_font_map();

         namespace fs = cycfi::fs;
         if (fs::exists(user_fonts_path) && fs::is_directory(user_fonts_path))
         {
            for (auto const& entry : fs::directory_iterator(user_fonts_path))
            {
               auto path = entry.path();
               auto ext = path.extension().string();
               if (ext != ".ttf" && ext != ".otf" && ext != ".ttc")
                  continue;

               int face_count = 0;
               // Count faces in the file by trying indices until we get null
               do
               {
                  auto tf = sk_sp<SkTypeface>(mgr->makeFromFile(path.string().c_str(), face_count));
                  if (!tf)
                     break;

                  SkString family_name;
                  tf->getFamilyName(&family_name);
                  std::string key = family_name.c_str();
                  trim(key);

                  auto sk_style = tf->fontStyle();
                  font_entry fe;
                  fe.file  = path.string();
                  fe.index = face_count;
                  // Map SkFontStyle weight (100-900) to artist 0-100 scale
                  fe.weight  = uint8_t(sk_style.weight() / 10);
                  // Map SkFontStyle slant: upright=0, italic=1, oblique=2
                  fe.slant   = uint8_t(sk_style.slant() == SkFontStyle::kItalic_Slant ? 100 :
                                       sk_style.slant() == SkFontStyle::kOblique_Slant ? 50 : 0);
                  // Map SkFontStyle width (1-9) to artist 0-100 scale
                  fe.stretch = uint8_t(sk_style.width() * 100 / 9);

                  // Register under the family name the font manager reports
                  // (DirectWrite: legacy name-ID-1 family; CoreText: typographic
                  // name-ID-16 family) AND under the typographic family read
                  // straight from the 'name' table, so a lookup by either name
                  // succeeds on every platform — matching fontconfig, which
                  // exposes both. e.g. "Open Sans Condensed Light" (ID 1) and
                  // "Open Sans Condensed" (ID 16).
                  std::string typo = get_name_id(tf.get(), 16);
                  trim(typo);

                  font_map[key].push_back(fe);
                  if (!typo.empty() && typo != key)
                     font_map[typo].push_back(fe);
                  ++face_count;
               } while (true);
            }
         }
#elif !defined(_WIN32)
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
            FcChar8 *file, *family;
            int index;
            if (FcPatternGetString(font, FC_FILE, 0, &file) == FcResultMatch &&
               FcPatternGetString(font, FC_FAMILY, 0, &family) == FcResultMatch &&
               FcPatternGetInteger(font, FC_INDEX, 0, &index) == FcResultMatch
            )
            {
               font_entry entry;
               entry.cached_typeface = nullptr;
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

               font_map[key].push_back(std::move(entry));
            }
         }
#endif
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

   font::font()
    : _ptr(std::make_shared<SkFont>())
   {
   }

   font::font(font_descr descr)
   {
      auto [font_map, font_map_mutex] = get_font_map();
      std::lock_guard<std::mutex> lock(font_map_mutex);

      auto match_ptr = match(font_map, descr);
      if (match_ptr)
      {
         if (match_ptr->cached_typeface)
         {
            _ptr = std::make_shared<SkFont>(match_ptr->cached_typeface, descr._size);
         }
         else
         {
            auto face = get_font_mgr()->makeFromFile(match_ptr->file.c_str(), match_ptr->index);
            _ptr = std::make_shared<SkFont>(face, descr._size);
            if (_ptr)
               match_ptr->cached_typeface = sk_ref_sp(_ptr->getTypeface());
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

      auto mgr = get_font_mgr();
      auto default_face = sk_sp<SkTypeface>(mgr->matchFamilyStyle(nullptr, style));
      std::istringstream str(std::string{descr._families});
      std::string family;

      while (getline(str, family, ','))
      {
         trim(family);
         auto face = sk_sp<SkTypeface>(mgr->matchFamilyStyle(family.c_str(), style));
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
      return {-sk_metrics.fAscent, sk_metrics.fDescent, sk_metrics.fLeading};
   }

   float font::measure_text(std::string_view str) const
   {
      return _ptr->measureText(str.data(), str.size(), SkTextEncoding::kUTF8);
   }
}


