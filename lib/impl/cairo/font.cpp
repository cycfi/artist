/*=============================================================================
   Copyright (c) 2016-2024 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
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

#ifdef __APPLE__
#  include <cairo-quartz.h>
#  include <CoreGraphics/CoreGraphics.h>
#  include <ft2build.h>
#  include FT_FREETYPE_H
#endif

namespace cycfi::artist
{
   namespace
   {
      inline void trim(std::string& s)
      {
         auto notpad = [](int ch){ return ch != ' ' && ch != '"'; };
         s.erase(s.begin(), std::find_if(s.begin(), s.end(), notpad));
         s.erase(std::find_if(s.rbegin(), s.rend(), notpad).base(), s.end());
      }

      enum fc_weight_vals
      {
         fc_thin       = 0,
         fc_extralight = 40,
         fc_light      = 50,
         fc_normal     = 80,
         fc_medium     = 100,
         fc_semibold   = 180,
         fc_bold       = 200,
         fc_extrabold  = 205,
         fc_black      = 210,
      };

      int to_fc_weight(int w)
      {
         namespace fc = font_constants;
         auto lerp = [](double a, double b, double t){ return a + (b-a)*t; };
         auto seg  = [&](int amin, int amax, int fmin, int fmax) -> double {
            return lerp(fmin, fmax, double(w-amin)/(amax-amin));
         };
         if (w <= fc::thin)          return fc_thin;
         if (w <= fc::extra_light)   return int(seg(fc::thin,         fc::extra_light,  fc_thin,      fc_extralight));
         if (w <= fc::light)         return int(seg(fc::extra_light,  fc::light,        fc_extralight, fc_light));
         if (w <= fc::weight_normal) return int(seg(fc::light,        fc::weight_normal,fc_light,      fc_normal));
         if (w <= fc::medium)        return int(seg(fc::weight_normal,fc::medium,       fc_normal,     fc_medium));
         if (w <= fc::semi_bold)     return int(seg(fc::medium,       fc::semi_bold,    fc_medium,     fc_semibold));
         if (w <= fc::bold)          return int(seg(fc::semi_bold,    fc::bold,         fc_semibold,   fc_bold));
         if (w <= fc::extra_bold)    return int(seg(fc::bold,         fc::extra_bold,   fc_bold,       fc_extrabold));
         return int(seg(fc::extra_bold, 100, fc_extrabold, fc_black));
      }

      struct fc_state
      {
         FcConfig* config = nullptr;
         fc_state()
         {
            FcInit();
            config = FcConfigGetCurrent();
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

      FcPattern* fc_match(font_descr const& descr)
      {
         FcConfig* cfg = get_fc_config();
         std::istringstream fs(std::string{descr._families});
         std::string family;
         while (std::getline(fs, family, ','))
         {
            trim(family);
            if (family.empty()) continue;
            FcPattern* pat = FcPatternCreate();
            FcPatternAddString(pat,  FC_FAMILY,
               reinterpret_cast<FcChar8 const*>(family.c_str()));
            FcPatternAddDouble(pat,  FC_SIZE,   descr._size);
            FcPatternAddInteger(pat, FC_WEIGHT, to_fc_weight(descr._weight));
            FcPatternAddInteger(pat, FC_SLANT,
               (descr._slant == font_constants::italic)  ? FC_SLANT_ITALIC  :
               (descr._slant == font_constants::oblique) ? FC_SLANT_OBLIQUE :
               FC_SLANT_ROMAN);
            FcPatternAddInteger(pat, FC_WIDTH, descr._stretch * 2);
            FcConfigSubstitute(cfg, pat, FcMatchPattern);
            FcDefaultSubstitute(pat);
            FcResult result;
            FcPattern* match = FcFontMatch(cfg, pat, &result);
            FcPatternDestroy(pat);
            if (match && result == FcResultMatch) return match;
            if (match) FcPatternDestroy(match);
         }
         FcPattern* pat = FcPatternCreate();
         FcPatternAddDouble(pat, FC_SIZE, descr._size);
         FcConfigSubstitute(cfg, pat, FcMatchPattern);
         FcDefaultSubstitute(pat);
         FcResult result;
         FcPattern* match = FcFontMatch(cfg, pat, &result);
         FcPatternDestroy(pat);
         return match;
      }

      // Build a HarfBuzz font from a FreeType face.
      // hb_ft_face_create_referenced copies the font blob from ft_face so the
      // caller may destroy ft_face (and its library) immediately after.
      font_impl::hb_font_ptr make_hb_font(FT_Face ft_face)
      {
         hb_face_t* hb_face = hb_ft_face_create_referenced(ft_face);
         if (!hb_face) return nullptr;
         hb_font_t* raw = hb_font_create(hb_face);
         if (raw)
         {
            hb_ot_font_set_funcs(raw);
            unsigned upem = hb_face_get_upem(hb_face);
            hb_font_set_scale(raw, int(upem), int(upem));
         }
         hb_face_destroy(hb_face);
         return font_impl::hb_font_ptr(raw);
      }

      // Derive a correctly-sized cairo_scaled_font_t via a recording surface.
      // Matches the Elements scratch_context approach; works for both CG and
      // FreeType faces since set_font_face + set_font_size is surface-agnostic.
      cairo_scaled_font_t* make_scaled_font(cairo_font_face_t* face, float size)
      {
         auto* surf = cairo_recording_surface_create(CAIRO_CONTENT_COLOR_ALPHA, nullptr);
         auto* ctx  = cairo_create(surf);
         cairo_set_font_face(ctx, face);
         cairo_set_font_size(ctx, size);
         auto* sf = cairo_get_scaled_font(ctx);
         cairo_scaled_font_reference(sf);
         cairo_destroy(ctx);
         cairo_surface_destroy(surf);
         return sf;
      }

#ifdef __APPLE__
      font_impl* make_font_impl(font_descr const& descr)
      {
         FcPattern* fc_pat = fc_match(descr);
         if (!fc_pat) return nullptr;

         // Copy FC_FULLNAME before destroying the pattern.
         FcChar8* fullname_raw = nullptr;
         FcPatternGetString(fc_pat, FC_FULLNAME, 0, &fullname_raw);
         std::string full_name = fullname_raw ? (char*)fullname_raw : "";

         // FT-backed cairo face — used to create the FT scaled font (_scaled_font)
         // which serves both standalone metrics AND non-Quartz rendering (tests,
         // offscreen).  The FT face itself is not stored; only the scaled font is.
         auto* ft_face = cairo_ft_font_face_create_for_pattern(fc_pat);
         FcPatternDestroy(fc_pat);

         if (!ft_face || cairo_font_face_status(ft_face) != CAIRO_STATUS_SUCCESS)
         {
            if (ft_face) cairo_font_face_destroy(ft_face);
            return nullptr;
         }

         // FT scaled font: bake HINT_METRICS_OFF so metrics match the original
         // FT-based values expected by tests (CoreText and FT agree here).
         cairo_matrix_t fm, ctm;
         cairo_matrix_init_scale(&fm, descr._size, descr._size);
         cairo_matrix_init_identity(&ctm);
         auto* opts = cairo_font_options_create();
         cairo_font_options_set_antialias(opts,    CAIRO_ANTIALIAS_GRAY);
         cairo_font_options_set_hint_style(opts,   CAIRO_HINT_STYLE_NONE);
         cairo_font_options_set_hint_metrics(opts, CAIRO_HINT_METRICS_OFF);
         auto* sf = cairo_scaled_font_create(ft_face, &fm, &ctm, opts);
         cairo_font_options_destroy(opts);

         // Bootstrap HarfBuzz from the FT face locked via the scaled font.
         font_impl::hb_font_ptr hb_fnt;
         if (sf)
         {
            FT_Face hb_ft = cairo_ft_scaled_font_lock_face(sf);
            if (hb_ft)
            {
               hb_fnt = make_hb_font(hb_ft);
               cairo_ft_scaled_font_unlock_face(sf);
            }
         }

         cairo_font_face_destroy(ft_face);  // scaled font holds its own reference

         if (full_name.empty())
         {
            if (sf) cairo_scaled_font_destroy(sf);
            return nullptr;
         }

         // CG-backed Cairo face for Quartz surface rendering (isFlipped=YES).
         // CGFontCreateWithFontName requires the font to be registered with
         // Core Text (system fonts are; bundled fonts need
         // CTFontManagerRegisterFontsForURL, done in cairo_app.mm init_paths).
         auto cfstr  = CFStringCreateWithCString(
            kCFAllocatorDefault, full_name.c_str(), kCFStringEncodingUTF8);
         auto cgfont = CGFontCreateWithFontName(cfstr);
         CFRelease(cfstr);

         cairo_font_face_t* cg_face = nullptr;
         if (cgfont)
         {
            cg_face = cairo_quartz_font_face_create_for_cgfont(cgfont);
            CGFontRelease(cgfont);
            if (cg_face && cairo_font_face_status(cg_face) != CAIRO_STATUS_SUCCESS)
            {
               cairo_font_face_destroy(cg_face);
               cg_face = nullptr;
            }
         }
         // cg_face may be null if the font isn't registered; non-Quartz path
         // (cairo_set_scaled_font with the FT sf) remains fully functional.

         return new font_impl(cg_face, sf, descr._size, std::move(hb_fnt));
      }

#else
      font_impl* make_font_impl(font_descr const& descr)
      {
         FcPattern* fc_pat = fc_match(descr);
         if (!fc_pat) return nullptr;

         auto* face = cairo_ft_font_face_create_for_pattern(fc_pat);
         FcPatternDestroy(fc_pat);

         if (!face || cairo_font_face_status(face) != CAIRO_STATUS_SUCCESS)
         {
            if (face) cairo_font_face_destroy(face);
            return nullptr;
         }

         auto* sf = make_scaled_font(face, descr._size);
         cairo_font_face_destroy(face);

         if (!sf || cairo_scaled_font_status(sf) != CAIRO_STATUS_SUCCESS)
         {
            if (sf) cairo_scaled_font_destroy(sf);
            return nullptr;
         }

         font_impl::hb_font_ptr hb_fnt;
         FT_Face ft_face = cairo_ft_scaled_font_lock_face(sf);
         if (ft_face)
         {
            hb_fnt = make_hb_font(ft_face);
            cairo_ft_scaled_font_unlock_face(sf);
         }

         // _face is null on non-macOS: cairo_set_scaled_font is used everywhere
         return new font_impl(nullptr, sf, descr._size, std::move(hb_fnt));
      }
#endif
   }

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
