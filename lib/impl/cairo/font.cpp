/*=============================================================================
   Copyright (c) 2016-2024 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
// Stage 5: Cairo font implementation via FreeType + Fontconfig.
// Uses cairo_ft_font_face_create_for_pattern to create a FreeType-backed
// font face, then cairo_scaled_font_new with the requested size.
// Font matching mirrors the Skia backend's Fontconfig approach.
#include "cairo_private.hpp"
#include "cairo_text.hpp"
#include <cairo/cairo-ft.h>
#include <fontconfig/fontconfig.h>
#include <hb-ft.h>
#include <hb-ot.h>
#include <infra/support.hpp>
#include <algorithm>
#include <sstream>
#include <string>

namespace cycfi::artist
{
   // font_impl is defined in cairo_private.hpp — included above.

   ////////////////////////////////////////////////////////////////////////////
   // Fontconfig helpers — mirrors the Skia backend's font-map approach.
   namespace
   {
      inline void trim(std::string& s)
      {
         auto notpad = [](int ch){ return ch != ' ' && ch != '"'; };
         s.erase(s.begin(), std::find_if(s.begin(), s.end(), notpad));
         s.erase(std::find_if(s.rbegin(), s.rend(), notpad).base(), s.end());
      }

      // Fontconfig weight scale: 0-210 maps to fc_thin..fc_black.
      // We re-use the same mapping constants as the Skia backend.
      enum fc_weight_vals
      {
         fc_thin      = 0,
         fc_extralight= 40,
         fc_light     = 50,
         fc_normal    = 80,
         fc_medium    = 100,
         fc_semibold  = 180,
         fc_bold      = 200,
         fc_extrabold = 205,
         fc_black     = 210,
      };

      // Map Artist weight (0–100 scale) to Fontconfig weight (0–210 scale).
      int to_fc_weight(int artist_weight)
      {
         namespace fc = font_constants;
         // Artist weight_normal=40; semibold=60; bold=70; black=90
         // Linear remap through same breakpoints as Skia's map_fc_weight inverse.
         auto lerp = [](double a, double b, double t){ return a + (b-a)*t; };
         auto seg = [&](int amin, int amax, int fmin, int fmax) -> double
         {
            return lerp(fmin, fmax, double(artist_weight-amin) / (amax-amin));
         };
         using namespace font_constants;
         if (artist_weight <= thin)         return fc_thin;
         if (artist_weight <= extra_light)  return int(seg(thin, extra_light,  fc_thin,      fc_extralight));
         if (artist_weight <= light)        return int(seg(extra_light, light, fc_extralight, fc_light));
         if (artist_weight <= weight_normal)return int(seg(light, weight_normal, fc_light,   fc_normal));
         if (artist_weight <= medium)       return int(seg(weight_normal, medium, fc_normal, fc_medium));
         if (artist_weight <= semi_bold)    return int(seg(medium, semi_bold, fc_medium,     fc_semibold));
         if (artist_weight <= bold)         return int(seg(semi_bold, bold, fc_semibold,     fc_bold));
         if (artist_weight <= extra_bold)   return int(seg(bold, extra_bold, fc_bold,        fc_extrabold));
         return int(seg(extra_bold, 100, fc_extrabold, fc_black));
      }

      struct fc_state
      {
         FcConfig* config = nullptr;

         fc_state()
         {
            FcInit();
            config = FcConfigGetCurrent();
            // Register user fonts directory so test fonts (Open Sans) are found.
            auto user_path = get_user_fonts_directory();
            FcConfigAppFontAddDir(config,
               reinterpret_cast<FcChar8 const*>(user_path.string().c_str()));
         }
      };

      FcConfig* get_fc_config()
      {
         static fc_state state;
         return state.config;
      }

      // Match font_descr against Fontconfig and return an FcPattern* with the
      // full font information (caller must FcPatternDestroy when done).
      // Returns nullptr if nothing is found.
      FcPattern* fc_match(font_descr const& descr)
      {
         FcConfig* cfg = get_fc_config();

         // Try each family in the comma-separated list.
         std::istringstream families_stream(std::string{descr._families});
         std::string family;
         while (std::getline(families_stream, family, ','))
         {
            trim(family);
            if (family.empty()) continue;

            FcPattern* pat = FcPatternCreate();
            FcPatternAddString(pat, FC_FAMILY,
               reinterpret_cast<FcChar8 const*>(family.c_str()));
            FcPatternAddDouble(pat, FC_SIZE, descr._size);
            FcPatternAddInteger(pat, FC_WEIGHT, to_fc_weight(descr._weight));
            FcPatternAddInteger(pat, FC_SLANT,
               (descr._slant == font_constants::italic) ? FC_SLANT_ITALIC :
               (descr._slant == font_constants::oblique)? FC_SLANT_OBLIQUE :
               FC_SLANT_ROMAN);
            // Map Artist stretch to Fontconfig width (stretch_normal=50 → FC_WIDTH_NORMAL=100)
            FcPatternAddInteger(pat, FC_WIDTH, descr._stretch * 2);

            FcConfigSubstitute(cfg, pat, FcMatchPattern);
            FcDefaultSubstitute(pat);

            FcResult result;
            FcPattern* match = FcFontMatch(cfg, pat, &result);
            FcPatternDestroy(pat);

            if (match && result == FcResultMatch)
               return match;
            if (match)
               FcPatternDestroy(match);
         }

         // Fallback: use system default font.
         FcPattern* pat = FcPatternCreate();
         FcPatternAddDouble(pat, FC_SIZE, descr._size);
         FcConfigSubstitute(cfg, pat, FcMatchPattern);
         FcDefaultSubstitute(pat);
         FcResult result;
         FcPattern* match = FcFontMatch(cfg, pat, &result);
         FcPatternDestroy(pat);
         return match; // may be nullptr
      }

      // Create a font_impl from a font_descr: Cairo scaled font + HarfBuzz font.
      // Returns nullptr if font matching or creation fails.
      font_impl* make_font_impl(font_descr const& descr)
      {
         FcPattern* fc_pat = fc_match(descr);
         if (!fc_pat)
            return nullptr;

         cairo_font_face_t* face = cairo_ft_font_face_create_for_pattern(fc_pat);
         FcPatternDestroy(fc_pat);

         if (!face || cairo_font_face_status(face) != CAIRO_STATUS_SUCCESS)
         {
            if (face) cairo_font_face_destroy(face);
            return nullptr;
         }

         cairo_matrix_t font_matrix, ctm;
         cairo_matrix_init_scale(&font_matrix, descr._size, descr._size);
         cairo_matrix_init_identity(&ctm);

         cairo_font_options_t* opts = cairo_font_options_create();
         cairo_font_options_set_antialias(opts, CAIRO_ANTIALIAS_GRAY);
         cairo_font_options_set_hint_style(opts, CAIRO_HINT_STYLE_NONE);
         // HINT_METRICS_OFF: keep fractional metrics so floor() comparisons in
         // tests match CoreText values rather than being rounded up to integers.
         cairo_font_options_set_hint_metrics(opts, CAIRO_HINT_METRICS_OFF);

         cairo_scaled_font_t* sf = cairo_scaled_font_create(face, &font_matrix, &ctm, opts);

         cairo_font_options_destroy(opts);
         cairo_font_face_destroy(face);

         if (!sf || cairo_scaled_font_status(sf) != CAIRO_STATUS_SUCCESS)
         {
            if (sf) cairo_scaled_font_destroy(sf);
            return nullptr;
         }

         // Create HarfBuzz font via hb_ft_face_create_referenced + hb_ot_font_set_funcs.
         //
         // Do NOT use hb_ft_font_create_referenced: that keeps the FT_Face live for
         // metric queries during hb_shape, but Cairo's FT_Face is shared and its size
         // state changes between canvas operations.  hb_ot_font_set_funcs reads glyph
         // advances directly from the OpenType hmtx/GSUB/GPOS tables — no dependency
         // on the FT_Face's current ppem — and activates GSUB lookups (liga, etc.).
         font_impl::hb_font_ptr hb_fnt;
         FT_Face ft_face = cairo_ft_scaled_font_lock_face(sf);
         if (ft_face)
         {
            // hb_ft_face_create_referenced copies/references the font blob from the
            // FT_Face — safe to unlock immediately after.
            hb_face_t* hb_face = hb_ft_face_create_referenced(ft_face);
            cairo_ft_scaled_font_unlock_face(sf);
            if (hb_face)
            {
               hb_font_t* raw = hb_font_create(hb_face);
               if (raw)
               {
                  // OT funcs read metrics from OpenType tables; scale at upem so
                  // positions are in font units.  Pixel value = hb_value*(size/upem).
                  hb_ot_font_set_funcs(raw);
                  unsigned upem = hb_face_get_upem(hb_face);
                  hb_font_set_scale(raw, int(upem), int(upem));
                  hb_fnt.reset(raw);
               }
               hb_face_destroy(hb_face);
            }
         }
         else
         {
            cairo_ft_scaled_font_unlock_face(sf);
         }

         return new font_impl(sf, descr._size, std::move(hb_fnt));
      }
   }

   ////////////////////////////////////////////////////////////////////////////
   font::font()
    : _ptr(new font_impl)
   {
   }

   font::font(font_descr descr)
    : _ptr(make_font_impl(descr))
   {
      if (!_ptr) _ptr = new font_impl;
   }

   font::font(font const& rhs)
    : _ptr(new font_impl(*rhs._ptr))
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
      if (!_ptr || !_ptr->_scaled_font)
         return {};
      cairo_font_extents_t ext;
      cairo_scaled_font_extents(_ptr->_scaled_font, &ext);
      float leading = float(ext.height) - float(ext.ascent) - float(ext.descent);
      if (leading < 0) leading = 0;
      return {float(ext.ascent), float(ext.descent), leading};
   }

   float font::measure_text(std::string_view str) const
   {
      if (!_ptr || !_ptr->_hb_font)
         return 0.0f;
      return shape_text(_ptr->_hb_font.get(), _ptr->_size, str).advance_x;
   }
}
