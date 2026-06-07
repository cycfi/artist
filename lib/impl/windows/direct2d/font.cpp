/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]

   DirectWrite font: maps font_descr -> IDWriteTextFormat (+ IDWriteFontFace for
   metrics), and provides metrics / measure_text.
=============================================================================*/
#include <artist/font.hpp>
#include "font_impl.hpp"
#include <sstream>
#include <algorithm>
#include <cmath>

namespace cycfi::artist
{
   using namespace font_constants;

   namespace d2d
   {
      std::wstring to_utf16(std::string_view utf8)
      {
         if (utf8.empty())
            return {};
         int n = MultiByteToWideChar(
            CP_UTF8, 0, utf8.data(), int(utf8.size()), nullptr, 0);
         std::wstring out(n, L'\0');
         MultiByteToWideChar(
            CP_UTF8, 0, utf8.data(), int(utf8.size()), out.data(), n);
         return out;
      }

      std::wstring to_utf16(std::u32string_view utf32)
      {
         std::wstring out;
         out.reserve(utf32.size());
         for (char32_t cp : utf32)
         {
            if (cp <= 0xFFFF)
            {
               out.push_back(wchar_t(cp));
            }
            else
            {
               cp -= 0x10000;
               out.push_back(wchar_t(0xD800 + (cp >> 10)));
               out.push_back(wchar_t(0xDC00 + (cp & 0x3FF)));
            }
         }
         return out;
      }
   }

   namespace
   {
      void trim(std::string& s)
      {
         auto notspace = [](int ch){ return ch != ' ' && ch != '"'; };
         s.erase(s.begin(), std::find_if(s.begin(), s.end(), notspace));
         s.erase(std::find_if(s.rbegin(), s.rend(), notspace).base(), s.end());
      }

      DWRITE_FONT_WEIGHT dwrite_weight(uint8_t w)
      {
         // artist weight is 10..95 (×10 ≈ DWRITE 100..950)
         int v = int(w) * 10;
         return DWRITE_FONT_WEIGHT(std::clamp(v, 1, 999));
      }

      DWRITE_FONT_STYLE dwrite_style(font_constants::slant_enum s)
      {
         switch (s)
         {
            case italic:  return DWRITE_FONT_STYLE_ITALIC;
            case oblique: return DWRITE_FONT_STYLE_OBLIQUE;
            default:      return DWRITE_FONT_STYLE_NORMAL;
         }
      }

      DWRITE_FONT_STRETCH dwrite_stretch(uint8_t st)
      {
         if (st <= condensed)       return DWRITE_FONT_STRETCH_CONDENSED;
         else if (st >= expanded)   return DWRITE_FONT_STRETCH_EXPANDED;
         return DWRITE_FONT_STRETCH_NORMAL;
      }

      // Resolve a font face for metrics (and confirm the family).
      void resolve(
         std::wstring const& family
       , DWRITE_FONT_WEIGHT weight, DWRITE_FONT_STYLE style, DWRITE_FONT_STRETCH stretch
       , IDWriteFontFace** face_out)
      {
         IDWriteFontCollection* coll = nullptr;
         if (FAILED(d2d::dwrite_factory()->GetSystemFontCollection(&coll)))
            return;
         UINT32 index = 0;
         BOOL exists = FALSE;
         coll->FindFamilyName(family.c_str(), &index, &exists);
         if (!exists)
            index = 0;   // fall back to the first family

         IDWriteFontFamily* fam = nullptr;
         if (SUCCEEDED(coll->GetFontFamily(index, &fam)) && fam)
         {
            IDWriteFont* fnt = nullptr;
            if (SUCCEEDED(fam->GetFirstMatchingFont(weight, stretch, style, &fnt)) && fnt)
            {
               fnt->CreateFontFace(face_out);
               d2d::release(fnt);
            }
            d2d::release(fam);
         }
         d2d::release(coll);
      }
   }

   font::font()
    : _ptr(nullptr)
   {
   }

   font::font(font_descr descr)
    : _ptr(new font_impl)
   {
      auto weight  = dwrite_weight(descr._weight);
      auto style   = dwrite_style(descr._slant);
      auto stretch = dwrite_stretch(descr._stretch);

      // First family in the comma-separated list (DirectWrite resolves a single
      // family; falls back internally if absent).
      std::istringstream str{std::string{descr._families}};
      std::string first;
      std::getline(str, first, ',');
      trim(first);
      std::wstring wfamily = d2d::to_utf16(std::string_view{first});

      _ptr->size = descr._size;
      d2d::dwrite_factory()->CreateTextFormat(
         wfamily.c_str(), nullptr, weight, style, stretch,
         descr._size, L"en-us", &_ptr->format
      );

      resolve(wfamily, weight, style, stretch, &_ptr->face);
      if (_ptr->face)
      {
         DWRITE_FONT_METRICS fm{};
         _ptr->face->GetMetrics(&fm);
         float s = descr._size / float(fm.designUnitsPerEm);
         _ptr->ascent  = fm.ascent * s;
         _ptr->descent = fm.descent * s;
         _ptr->leading = fm.lineGap * s;
      }
   }

   font::font(font const& rhs)
    : _ptr(nullptr)
   {
      if (rhs._ptr)
      {
         _ptr = new font_impl(*rhs._ptr);
         if (_ptr->format) _ptr->format->AddRef();
         if (_ptr->face)   _ptr->face->AddRef();
      }
   }

   font::font(font&& rhs) noexcept
    : _ptr(rhs._ptr)
   {
      rhs._ptr = nullptr;
   }

   font::~font()
   {
      if (_ptr)
      {
         d2d::release(_ptr->format);
         d2d::release(_ptr->face);
         delete _ptr;
      }
   }

   font& font::operator=(font const& rhs)
   {
      if (this != &rhs)
      {
         font tmp{rhs};
         std::swap(_ptr, tmp._ptr);
      }
      return *this;
   }

   font& font::operator=(font&& rhs) noexcept
   {
      if (this != &rhs)
         std::swap(_ptr, rhs._ptr);
      return *this;
   }

   font::metrics_info font::metrics() const
   {
      if (!_ptr)
         return {};
      return {_ptr->ascent, _ptr->descent, _ptr->leading};
   }

   float font::measure_text(std::string_view str) const
   {
      if (!_ptr || !_ptr->format)
         return 0;
      auto wtext = d2d::to_utf16(str);
      IDWriteTextLayout* layout = nullptr;
      if (FAILED(d2d::dwrite_factory()->CreateTextLayout(
            wtext.c_str(), UINT32(wtext.size()), _ptr->format,
            FLT_MAX, FLT_MAX, &layout)) || !layout)
         return 0;
      DWRITE_TEXT_METRICS tm{};
      layout->GetMetrics(&tm);
      d2d::release(layout);
      return tm.widthIncludingTrailingWhitespace;
   }
}
