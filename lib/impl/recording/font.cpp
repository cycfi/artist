/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]

   Fonts for the recording backend. Families resolve through fontconfig with
   the bundled fonts registered, as on the Cairo backend. Metrics are the
   unhinted design metrics FreeType reports, scaled to the size, which is
   what the Cairo backend measures with. Shaping is HarfBuzz.
=============================================================================*/
#include "recording_impl.hpp"
#include <fontconfig/fontconfig.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb-ot.h>
#include <infra/utf8_utils.hpp>
#include <algorithm>
#include <cstdlib>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

namespace cycfi::artist
{
   namespace
   {
      void trim(std::string& s)
      {
         auto notpad = [](int ch){ return ch != ' ' && ch != '"'; };
         s.erase(s.begin(), std::find_if(s.begin(), s.end(), notpad));
         s.erase(std::find_if(s.rbegin(), s.rend(), notpad).base(), s.end());
      }

      // Fontconfig weight and slant, onto Artist's scales.
      int map_fc_weight(int w)
      {
         namespace fc = font_constants;
         enum { fc_thin = 0, fc_extralight = 40, fc_light = 50, fc_normal = 80,
                fc_medium = 100, fc_semibold = 180, fc_bold = 200,
                fc_extrabold = 205, fc_black = 210 };

         auto seg = [w](int f0, int f1, int a0, int a1)
         {
            return int(a0 + (a1 - a0) * double(w - f0) / (f1 - f0));
         };

         if (w <= fc_thin)       return fc::thin;
         if (w <= fc_extralight) return seg(fc_thin, fc_extralight, fc::thin, fc::extra_light);
         if (w <= fc_light)      return seg(fc_extralight, fc_light, fc::extra_light, fc::light);
         if (w <= fc_normal)     return seg(fc_light, fc_normal, fc::light, fc::weight_normal);
         if (w <= fc_medium)     return seg(fc_normal, fc_medium, fc::weight_normal, fc::medium);
         if (w <= fc_semibold)   return seg(fc_medium, fc_semibold, fc::medium, fc::semi_bold);
         if (w <= fc_bold)       return seg(fc_semibold, fc_bold, fc::semi_bold, fc::bold);
         if (w <= fc_extrabold)  return seg(fc_bold, fc_extrabold, fc::bold, fc::extra_bold);
         return seg(fc_extrabold, fc_black, fc::extra_bold, 90);
      }

      int map_fc_slant(int s)
      {
         namespace fc = font_constants;
         if (s == FC_SLANT_ITALIC)  return fc::italic;
         if (s == FC_SLANT_OBLIQUE) return fc::oblique;
         return fc::slant_normal;
      }

      ////////////////////////////////////////////////////////////////////////
      // Every installed and bundled font, enumerated once, keyed by family.
      struct font_entry
      {
         std::string          full_name;
         std::string          file;
         uint8_t              weight = font_constants::weight_normal;
         uint8_t              slant = font_constants::slant_normal;
         uint8_t              stretch = font_constants::stretch_normal;
      };

      using font_map_t = std::map<std::string, std::vector<font_entry>>;

      font_map_t& font_map()
      {
         static font_map_t map;
         return map;
      }

      void init_font_map()
      {
         FcInit();
         FcConfig* cfg = FcConfigGetCurrent();
         auto user_path = get_user_fonts_directory();
         FcConfigAppFontAddDir(cfg, reinterpret_cast<FcChar8 const*>(user_path.string().c_str()));

         FcObjectSet* os = FcObjectSetBuild(
            FC_FAMILY, FC_FULLNAME, FC_FILE, FC_WEIGHT, FC_SLANT, FC_WIDTH, nullptr);
         FcPattern* pat = FcPatternCreate();
         FcFontSet* fs = FcFontList(cfg, pat, os);
         FcPatternDestroy(pat);
         FcObjectSetDestroy(os);
         if (!fs)
            return;

         auto& map = font_map();
         for (int i = 0; i < fs->nfont; ++i)
         {
            FcPattern* p = fs->fonts[i];
            FcChar8* family, *full_name, *file;
            if (FcPatternGetString(p, FC_FAMILY, 0, &family) != FcResultMatch ||
               FcPatternGetString(p, FC_FULLNAME, 0, &full_name) != FcResultMatch ||
               FcPatternGetString(p, FC_FILE, 0, &file) != FcResultMatch)
               continue;

            font_entry e;
            e.full_name = reinterpret_cast<char const*>(full_name);
            e.file = reinterpret_cast<char const*>(file);

            int fc_weight = FC_WEIGHT_NORMAL;
            FcPatternGetInteger(p, FC_WEIGHT, 0, &fc_weight);
            e.weight = uint8_t(map_fc_weight(fc_weight));

            int fc_slant = FC_SLANT_ROMAN;
            FcPatternGetInteger(p, FC_SLANT, 0, &fc_slant);
            e.slant = uint8_t(map_fc_slant(fc_slant));

            int fc_width = FC_WIDTH_NORMAL;
            FcPatternGetInteger(p, FC_WIDTH, 0, &fc_width);
            e.stretch = uint8_t((fc_width * 100) / 200);

            std::string key = reinterpret_cast<char const*>(family);
            trim(key);
            map[key].push_back(std::move(e));
         }
         FcFontSetDestroy(fs);
      }

      std::mutex& font_mutex()
      {
         static std::mutex m;
         return m;
      }

      // Call with font_mutex held.
      font_entry const* match(font_descr const& descr)
      {
         static bool initialized = false;
         if (!initialized)
         {
            init_font_map();
            initialized = true;
         }

         auto& map = font_map();
         std::istringstream families(std::string{descr._families});
         std::string family;
         while (std::getline(families, family, ','))
         {
            trim(family);
            auto it = map.find(family);
            if (it == map.end())
               continue;

            // Slant matters most, then weight, then stretch.
            double best_score = 1e9;
            font_entry const* best = nullptr;
            for (auto const& e : it->second)
            {
               double score =
                  std::abs(int(descr._weight) - int(e.weight)) * 1.0 +
                  std::abs(int(descr._slant) - int(e.slant)) * 3.0 +
                  std::abs(int(descr._stretch) - int(e.stretch)) * 0.25;
               if (score < best_score)
               {
                  best_score = score;
                  best = &e;
               }
            }
            if (best)
               return best;
         }
         return nullptr;
      }

      ////////////////////////////////////////////////////////////////////////
      // One face per font file: the HarfBuzz face, and the design metrics
      // read once through FreeType, which is then let go.
      struct face_entry
      {
         hb_face_t*           hb_face = nullptr;
         float                upem = 0;
         float                ascender = 0;
         float                descender = 0;   // positive, below the baseline
         float                height = 0;
      };

      struct face_cache
      {
         std::map<std::string, face_entry> faces;

         ~face_cache()
         {
            for (auto& [file, e] : faces)
               if (e.hb_face)
                  hb_face_destroy(e.hb_face);
         }
      };

      face_entry const* get_face(std::string const& file)
      {
         static face_cache cache;
         auto it = cache.faces.find(file);
         if (it != cache.faces.end())
            return it->second.hb_face? &it->second : nullptr;

         face_entry e;
         FT_Library lib = nullptr;
         if (FT_Init_FreeType(&lib) == 0)
         {
            FT_Face face = nullptr;
            if (FT_New_Face(lib, file.c_str(), 0, &face) == 0)
            {
               if (FT_IS_SCALABLE(face) && face->units_per_EM > 0)
               {
                  e.upem = face->units_per_EM;
                  e.ascender = face->ascender;
                  e.descender = -face->descender;
                  e.height = face->height;
               }
               FT_Done_Face(face);
            }
            FT_Done_FreeType(lib);
         }

         if (e.upem > 0)
         {
            if (hb_blob_t* blob = hb_blob_create_from_file(file.c_str()))
            {
               e.hb_face = hb_face_create(blob, 0);
               hb_blob_destroy(blob);
            }
         }

         auto& slot = cache.faces[file];
         slot = e;
         return slot.hb_face? &slot : nullptr;
      }

      font_impl* make_font_impl(font_descr const& descr)
      {
         std::lock_guard lock(font_mutex());
         auto const* entry = match(descr);
         if (!entry)
            return nullptr;
         auto const* face = get_face(entry->file);
         if (!face)
            return nullptr;

         auto* fi = new font_impl;
         fi->size = descr._size;
         fi->hb = hb_font_create(face->hb_face);
         hb_ot_font_set_funcs(fi->hb);
         hb_font_set_scale(fi->hb, int(face->upem), int(face->upem));

         float k = descr._size / face->upem;
         fi->ascent = face->ascender * k;
         fi->descent = face->descender * k;
         fi->leading = std::max(0.0f, face->height * k - fi->ascent - fi->descent);
         return fi;
      }
   }

   font_impl::font_impl(font_impl const& rhs)
    : hb(rhs.hb? hb_font_reference(rhs.hb) : nullptr)
    , size(rhs.size)
    , ascent(rhs.ascent)
    , descent(rhs.descent)
    , leading(rhs.leading)
   {
   }

   font_impl::~font_impl()
   {
      if (hb)
         hb_font_destroy(hb);
   }

   ////////////////////////////////////////////////////////////////////////////
   font::font()
    : _ptr(new font_impl)
   {
   }

   font::font(font_descr descr)
    : _ptr(make_font_impl(descr))
   {
      if (!_ptr)
         _ptr = new font_impl;
   }

   font::font(font const& rhs)
    : _ptr(rhs._ptr? new font_impl(*rhs._ptr) : new font_impl)
   {
   }

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
         _ptr = rhs._ptr? new font_impl(*rhs._ptr) : new font_impl;
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
      if (!_ptr || !_ptr->hb)
         return {};
      return {_ptr->ascent, _ptr->descent, _ptr->leading};
   }

   float font::measure_text(std::string_view str) const
   {
      return recording::text_advance(*this, str);
   }
}

namespace cycfi::artist::recording
{
   shaped_run shape(font const& f, std::u32string_view utf32)
   {
      auto const* fi = f.impl();
      if (!fi || !fi->hb || utf32.empty())
         return {};

      hb_buffer_t* buf = hb_buffer_create();
      hb_buffer_add_utf32(buf,
         reinterpret_cast<uint32_t const*>(utf32.data()),
         int(utf32.size()), 0, int(utf32.size()));
      hb_buffer_guess_segment_properties(buf);
      hb_shape(fi->hb, buf, nullptr, 0);

      unsigned count;
      hb_glyph_info_t* infos = hb_buffer_get_glyph_infos(buf, &count);
      hb_glyph_position_t* poses = hb_buffer_get_glyph_positions(buf, &count);

      int sx, sy;
      hb_font_get_scale(fi->hb, &sx, &sy);
      float k = sx > 0? fi->size / float(sx) : 1.0f;

      shaped_run run;
      run.glyphs.reserve(count);
      for (unsigned i = 0; i != count; ++i)
      {
         shaped_glyph g{infos[i].cluster, poses[i].x_advance * k, poses[i].x_offset * k};
         run.glyphs.push_back(g);
         run.advance += g.x_advance;
      }
      hb_buffer_destroy(buf);
      return run;
   }

   float text_advance(font const& f, std::string_view utf8)
   {
      return shape(f, to_utf32(utf8)).advance;
   }
}
