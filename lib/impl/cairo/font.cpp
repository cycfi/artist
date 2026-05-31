/*=============================================================================
   Copyright (c) 2016-2024 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include "cairo_private.hpp"
#include "cairo_text.hpp"
#include <cairo/cairo-ft.h>
#include <fontconfig/fontconfig.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb.h>
#include <hb-ot.h>
#include <infra/support.hpp>
#include <algorithm>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

#ifdef __APPLE__
#  include <cairo-quartz.h>
#  include <CoreGraphics/CoreGraphics.h>
#endif

namespace cycfi::artist
{
   namespace
   {
      ///////////////////////////////////////////////////////////////////////////
      // Helpers

      inline void trim(std::string& s)
      {
         auto notpad = [](int ch){ return ch != ' ' && ch != '"'; };
         s.erase(s.begin(), std::find_if(s.begin(), s.end(), notpad));
         s.erase(std::find_if(s.rbegin(), s.rend(), notpad).base(), s.end());
      }

      ///////////////////////////////////////////////////////////////////////////
      // Fontconfig weight/slant/stretch → Artist scale conversions.
      // These are the inverse of the Artist→FC maps used for FcFontMatch.

      int map_fc_weight(int w)
      {
         // Fontconfig: 0=Thin .. 210=Black
         // Artist:    10=thin .. 90=black (see font_constants)
         namespace fc = font_constants;
         enum fc_vals { fc_thin=0, fc_extralight=40, fc_light=50, fc_normal=80,
                        fc_medium=100, fc_semibold=180, fc_bold=200,
                        fc_extrabold=205, fc_black=210 };

         auto lerp = [](double a, double b, double t){ return int(a + (b-a)*t); };
         auto seg  = [&](int f0, int f1, int a0, int a1) -> int {
            return lerp(a0, a1, double(w-f0) / (f1-f0));
         };

         if (w <= fc_thin)       return fc::thin;
         if (w <= fc_extralight) return seg(fc_thin,       fc_extralight, fc::thin,          fc::extra_light);
         if (w <= fc_light)      return seg(fc_extralight, fc_light,      fc::extra_light,   fc::light);
         if (w <= fc_normal)     return seg(fc_light,      fc_normal,     fc::light,         fc::weight_normal);
         if (w <= fc_medium)     return seg(fc_normal,     fc_medium,     fc::weight_normal, fc::medium);
         if (w <= fc_semibold)   return seg(fc_medium,     fc_semibold,   fc::medium,        fc::semi_bold);
         if (w <= fc_bold)       return seg(fc_semibold,   fc_bold,       fc::semi_bold,     fc::bold);
         if (w <= fc_extrabold)  return seg(fc_bold,       fc_extrabold,  fc::bold,          fc::extra_bold);
         return seg(fc_extrabold, fc_black, fc::extra_bold, 90);
      }

      int map_fc_slant(int s)
      {
         namespace fc = font_constants;
         if (s == FC_SLANT_ITALIC)  return fc::italic;
         if (s == FC_SLANT_OBLIQUE) return fc::oblique;
         return fc::slant_normal;
      }

      ///////////////////////////////////////////////////////////////////////////
      // Font map — enumerate all fonts once at startup, then resolve
      // font_descr via a local std::map lookup (no FcFontMatch per call).

      struct font_entry
      {
         std::string full_name;  // FC_FULLNAME (also the CGFont name on macOS)
         std::string file;       // FC_FILE path for FT_New_Face
         uint8_t     weight  = font_constants::weight_normal;
         uint8_t     slant   = font_constants::slant_normal;
         uint8_t     stretch = font_constants::stretch_normal;
      };

      using font_map_t = std::map<std::string, std::vector<font_entry>>;

      std::pair<font_map_t&, std::mutex&> get_font_map()
      {
         static font_map_t map;
         static std::mutex mutex;
         return {map, mutex};
      }

      void init_font_map()
      {
         FcInit();
         FcConfig* cfg = FcConfigGetCurrent();

         // Register bundled/user fonts so they appear in FC_FILE results.
         auto user_path = get_user_fonts_directory();
         FcConfigAppFontAddDir(cfg,
            reinterpret_cast<FcChar8 const*>(user_path.string().c_str()));

         FcObjectSet* os = FcObjectSetBuild(
            FC_FAMILY, FC_FULLNAME, FC_FILE, FC_WEIGHT, FC_SLANT, FC_WIDTH, nullptr);
         FcPattern* pat = FcPatternCreate();
         FcFontSet* fs  = FcFontList(cfg, pat, os);
         FcPatternDestroy(pat);
         FcObjectSetDestroy(os);
         if (!fs) return;

         auto [map, mutex] = get_font_map();
         std::lock_guard lock(mutex);

         for (int i = 0; i < fs->nfont; ++i)
         {
            FcPattern* p = fs->fonts[i];
            FcChar8 *family_raw, *fullname_raw, *file_raw;
            if (FcPatternGetString(p, FC_FAMILY,   0, &family_raw)   != FcResultMatch) continue;
            if (FcPatternGetString(p, FC_FULLNAME, 0, &fullname_raw) != FcResultMatch) continue;
            if (FcPatternGetString(p, FC_FILE,     0, &file_raw)     != FcResultMatch) continue;

            font_entry e;
            e.full_name = reinterpret_cast<char const*>(fullname_raw);
            e.file      = reinterpret_cast<char const*>(file_raw);

            int fc_weight = FC_WEIGHT_NORMAL;
            FcPatternGetInteger(p, FC_WEIGHT, 0, &fc_weight);
            e.weight = uint8_t(map_fc_weight(fc_weight));

            int fc_slant = FC_SLANT_ROMAN;
            FcPatternGetInteger(p, FC_SLANT, 0, &fc_slant);
            e.slant = uint8_t(map_fc_slant(fc_slant));

            int fc_width = FC_WIDTH_NORMAL;  // 100
            FcPatternGetInteger(p, FC_WIDTH, 0, &fc_width);
            e.stretch = uint8_t((fc_width * 100) / 200);

            std::string key = reinterpret_cast<char const*>(family_raw);
            trim(key);
            map[key].push_back(std::move(e));
         }

         FcFontSetDestroy(fs);
      }

      font_entry const* match(font_descr const& descr)
      {
         static std::once_flag init_flag;
         std::call_once(init_flag, init_font_map);

         auto [map, mutex] = get_font_map();
         std::lock_guard lock(mutex);

         std::istringstream families(std::string{descr._families});
         std::string family;
         while (std::getline(families, family, ','))
         {
            trim(family);
            auto it = map.find(family);
            if (it == map.end()) continue;

            double best_score = 1e9;
            font_entry const* best = nullptr;
            for (auto const& e : it->second)
            {
               // Biased score: slant has highest weight (×3), then weight (×1),
               // then stretch (×0.25). Lower is better.
               double score =
                  std::abs(int(descr._weight)  - int(e.weight))  * 1.0  +
                  std::abs(int(descr._slant)   - int(e.slant))   * 3.0  +
                  std::abs(int(descr._stretch) - int(e.stretch)) * 0.25 ;
               if (score < best_score) { best_score = score; best = &e; }
            }
            if (best) return best;
         }
         return nullptr;
      }

      ///////////////////////////////////////////////////////////////////////////
      // FreeType library — single process-wide instance.

      const cairo_user_data_key_t& ft_user_data_key()
      {
         static const cairo_user_data_key_t key = {};
         return key;
      }

      void destroy_ft_face(void* face)
      {
         FT_Done_Face(reinterpret_cast<FT_Face>(face));
      }

      FT_Library get_ft_library()
      {
         struct ft_lib_t
         {
            FT_Library lib = nullptr;
            ft_lib_t()  { FT_Init_FreeType(&lib); }
            ~ft_lib_t() { if (lib) FT_Done_FreeType(lib); }
         };
         static ft_lib_t inst;
         return inst.lib;
      }

      // Load a FT_Face from a file path and wrap it in a cairo_font_face_t.
      // The FT_Face lifetime is tied to the Cairo face via user data.
      cairo_font_face_t* make_ft_cairo_face(std::string const& file)
      {
         FT_Face ft_face = nullptr;
         if (FT_New_Face(get_ft_library(), file.c_str(), 0, &ft_face) != 0)
            return nullptr;

         cairo_font_face_t* face =
            cairo_ft_font_face_create_for_ft_face(ft_face, 0);
         if (!face || cairo_font_face_status(face) != CAIRO_STATUS_SUCCESS)
         {
            FT_Done_Face(ft_face);
            if (face) cairo_font_face_destroy(face);
            return nullptr;
         }

         // Attach ft_face to the cairo face so it is freed when Cairo releases it.
         if (cairo_font_face_set_user_data(
               face, &ft_user_data_key(), ft_face, destroy_ft_face)
            != CAIRO_STATUS_SUCCESS)
         {
            cairo_font_face_destroy(face);
            FT_Done_Face(ft_face);
            return nullptr;
         }

         return face;
      }

      ///////////////////////////////////////////////////////////////////////////
      // HarfBuzz font construction
      //
      // We create the hb_face_t from the font file blob rather than from the
      // FT_Face. This decouples HarfBuzz from FreeType's lifecycle entirely:
      // hb_face_destroy at process exit only frees the blob (raw memory), not
      // an FT_Face — avoiding cross-dylib teardown ordering crashes.

      hb_face_t* make_hb_face(std::string const& file)
      {
         hb_blob_t* blob = hb_blob_create_from_file(file.c_str());
         if (!blob) return nullptr;
         hb_face_t* face = hb_face_create(blob, 0);
         hb_blob_destroy(blob);
         return face;
      }

      font_impl::hb_font_ptr make_hb_font(hb_face_t* hb_face)
      {
         hb_font_t* raw = hb_font_create(hb_face);
         if (!raw) return nullptr;
         hb_ot_font_set_funcs(raw);
         unsigned upem = hb_face_get_upem(hb_face);
         hb_font_set_scale(raw, int(upem), int(upem));
         return font_impl::hb_font_ptr(raw);
      }

      ///////////////////////////////////////////////////////////////////////////
      // Face cache — keyed by FC_FULLNAME, holds ref-counted Cairo + HarfBuzz
      // faces. Scaled fonts are created per-size outside the cache.

      struct face_entry
      {
         cairo_font_face_t* ft_face = nullptr;  // FT-backed (metrics + non-Quartz)
         cairo_font_face_t* cg_face = nullptr;  // CG-backed (macOS Quartz only)
         hb_face_t*         hb_face = nullptr;  // HarfBuzz face (ref-counted)
      };

      using face_map_t = std::map<std::string, face_entry>;

      std::pair<face_map_t&, std::mutex&> get_face_cache()
      {
         static face_map_t map;
         static std::mutex mutex;

         struct cleanup
         {
            face_map_t& map_;
            ~cleanup()
            {
               for (auto& [key, e] : map_)
               {
                  if (e.ft_face) cairo_font_face_destroy(e.ft_face);
                  if (e.cg_face) cairo_font_face_destroy(e.cg_face);
                  if (e.hb_face) hb_face_destroy(e.hb_face);
               }
            }
         };
         static cleanup cleanup_{map};
         return {map, mutex};
      }

      ///////////////////////////////////////////////////////////////////////////
      // Scaled font creation (per-size, from a cached FT cairo face)

      cairo_scaled_font_t* make_scaled_font(cairo_font_face_t* ft_face, float size)
      {
         cairo_matrix_t fm, ctm;
         cairo_matrix_init_scale(&fm, size, size);
         cairo_matrix_init_identity(&ctm);
         auto* opts = cairo_font_options_create();
         cairo_font_options_set_antialias(opts,    CAIRO_ANTIALIAS_GRAY);
         cairo_font_options_set_hint_style(opts,   CAIRO_HINT_STYLE_NONE);
         cairo_font_options_set_hint_metrics(opts, CAIRO_HINT_METRICS_OFF);
         auto* sf = cairo_scaled_font_create(ft_face, &fm, &ctm, opts);
         cairo_font_options_destroy(opts);
         return sf;
      }

      ///////////////////////////////////////////////////////////////////////////
      // make_font_impl — main font construction

      font_impl* make_font_impl(font_descr const& descr)
      {
         auto const* entry = match(descr);
         if (!entry) return nullptr;

         auto [face_map, face_mutex] = get_face_cache();
         std::lock_guard lock(face_mutex);

         auto it = face_map.find(entry->full_name);
         if (it == face_map.end())
         {
            face_entry fe;

            fe.ft_face = make_ft_cairo_face(entry->file);
            if (!fe.ft_face) return nullptr;

            // HarfBuzz face from the font file directly — no FT_Face reference.
            // This keeps HarfBuzz and FreeType lifecycle-independent.
            fe.hb_face = make_hb_face(entry->file);

#ifdef __APPLE__
            auto cfstr  = CFStringCreateWithCString(
               kCFAllocatorDefault, entry->full_name.c_str(), kCFStringEncodingUTF8);
            auto cgfont = CGFontCreateWithFontName(cfstr);
            CFRelease(cfstr);
            if (cgfont)
            {
               fe.cg_face = cairo_quartz_font_face_create_for_cgfont(cgfont);
               CGFontRelease(cgfont);
               if (fe.cg_face &&
                   cairo_font_face_status(fe.cg_face) != CAIRO_STATUS_SUCCESS)
               {
                  cairo_font_face_destroy(fe.cg_face);
                  fe.cg_face = nullptr;
               }
            }
#endif
            it = face_map.emplace(entry->full_name, std::move(fe)).first;
         }

         face_entry const& fe = it->second;
         if (!fe.ft_face) return nullptr;

         // Scaled font is per-size — cheap to create from the cached FT face.
         auto* sf = make_scaled_font(fe.ft_face, descr._size);
         if (!sf || cairo_scaled_font_status(sf) != CAIRO_STATUS_SUCCESS)
         {
            if (sf) cairo_scaled_font_destroy(sf);
            return nullptr;
         }

         // HarfBuzz font from cached hb_face — O(1).
         font_impl::hb_font_ptr hb_fnt;
         if (fe.hb_face)
            hb_fnt = make_hb_font(fe.hb_face);

         // CG face reference (macOS Quartz only, may be null).
         cairo_font_face_t* cg_face = fe.cg_face;
         if (cg_face) cairo_font_face_reference(cg_face);

         return new font_impl(cg_face, sf, descr._size, std::move(hb_fnt));
      }

   } // namespace

   ////////////////////////////////////////////////////////////////////////////
   font::font()
    : _ptr(new font_impl)
   {}

   font::font(font_descr descr)
    : _ptr(make_font_impl(descr))
   {
      if (!_ptr) _ptr = new font_impl;
   }

   font::font(font const& rhs)
    : _ptr(new font_impl(*rhs._ptr))
   {}

   font::font(font&& rhs) noexcept
    : _ptr(rhs._ptr)
   {
      rhs._ptr = nullptr;
   }

   font::~font()
   {
      delete _ptr;
   }

   font& font::operator=(font const& rhs)
   {
      if (this != &rhs)
      {
         delete _ptr;
         _ptr = new font_impl(*rhs._ptr);
      }
      return *this;
   }

   font& font::operator=(font&& rhs) noexcept
   {
      if (this != &rhs)
      {
         delete _ptr;
         _ptr = rhs._ptr;
         rhs._ptr = nullptr;
      }
      return *this;
   }

   font::metrics_info font::metrics() const
   {
      if (!_ptr || !_ptr->_scaled_font) return {};
      cairo_font_extents_t ext;
      cairo_scaled_font_extents(_ptr->_scaled_font, &ext);
      float leading = float(ext.height) - float(ext.ascent) - float(ext.descent);
      if (leading < 0) leading = 0;
      return {float(ext.ascent), float(ext.descent), leading};
   }

   float font::measure_text(std::string_view str) const
   {
      if (!_ptr || !_ptr->_scaled_font) return 0.0f;
      cairo_text_extents_t ext;
      cairo_scaled_font_text_extents(_ptr->_scaled_font,
         std::string{str}.c_str(), &ext);
      return float(ext.x_advance);
   }
}
