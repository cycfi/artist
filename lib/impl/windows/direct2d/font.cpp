/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]

   DirectWrite font: maps font_descr -> IDWriteTextFormat (+ IDWriteFontFace for
   metrics), and provides metrics / measure_text.
=============================================================================*/
#include <artist/font.hpp>
#include "font_impl.hpp"
#include <dwrite_3.h>
#include <infra/filesystem.hpp>
#include <sstream>
#include <algorithm>
#include <cctype>
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

      // Process-wide custom font collection built from the font files in
      // get_user_fonts_directory(). DirectWrite only exposes *system* fonts by
      // default, but Artist's tests/examples ship their fonts (Open Sans, …) as
      // .ttf in resources/fonts (the other backends load those files too). Read
      // them into a collection so font_descr family names resolve to the bundled
      // faces. Returns nullptr if there are none. (Mirrors skia/font.cpp.)
      IDWriteFontCollection* custom_font_collection()
      {
         static IDWriteFontCollection* coll = []() -> IDWriteFontCollection*
         {
            IDWriteFactory3* f3 = nullptr;
            if (FAILED(dwrite_factory()->QueryInterface(
                  __uuidof(IDWriteFactory3), reinterpret_cast<void**>(&f3))) || !f3)
               return nullptr;

            IDWriteFontSetBuilder* builder0 = nullptr;
            IDWriteFontSetBuilder1* builder = nullptr;
            if (FAILED(f3->CreateFontSetBuilder(&builder0)) || !builder0 ||
                FAILED(builder0->QueryInterface(
                  __uuidof(IDWriteFontSetBuilder1), reinterpret_cast<void**>(&builder))))
            {
               if (builder0) builder0->Release();
               f3->Release();
               return nullptr;
            }

            fs::path dir;
            try { dir = get_user_fonts_directory(); } catch (...) {}

            int added = 0;
            std::error_code ec;
            if (!dir.empty() && fs::exists(dir, ec) && fs::is_directory(dir, ec))
            {
               for (auto const& e : fs::directory_iterator(dir, ec))
               {
                  if (!e.is_regular_file(ec))
                     continue;
                  std::string ext = e.path().extension().string();
                  for (auto& ch : ext) ch = char(std::tolower((unsigned char)ch));
                  if (ext != ".ttf" && ext != ".otf" && ext != ".ttc")
                     continue;
                  IDWriteFontFile* file = nullptr;
                  std::wstring wp = e.path().wstring();
                  if (SUCCEEDED(f3->CreateFontFileReference(wp.c_str(), nullptr, &file)) && file)
                  {
                     if (SUCCEEDED(builder->AddFontFile(file)))
                        ++added;
                     file->Release();
                  }
               }
            }

            IDWriteFontCollection* result = nullptr;
            if (added)
            {
               IDWriteFontSet* set = nullptr;
               if (SUCCEEDED(builder0->CreateFontSet(&set)) && set)
               {
                  IDWriteFontCollection1* c1 = nullptr;
                  if (SUCCEEDED(f3->CreateFontCollectionFromFontSet(set, &c1)) && c1)
                     result = c1;   // IDWriteFontCollection1 : IDWriteFontCollection
                  set->Release();
               }
            }
            builder->Release();
            builder0->Release();
            f3->Release();
            return result;
         }();
         return coll;
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

      // Resolve a font face for metrics (and confirm the family). `coll` is the
      // collection the text format uses (custom bundled fonts, or nullptr for
      // the system collection).
      void resolve(
         IDWriteFontCollection* coll
       , std::wstring const& family
       , DWRITE_FONT_WEIGHT weight, DWRITE_FONT_STYLE style, DWRITE_FONT_STRETCH stretch
       , IDWriteFontFace** face_out)
      {
         IDWriteFontCollection* sys = nullptr;
         if (!coll)
         {
            if (FAILED(d2d::dwrite_factory()->GetSystemFontCollection(&sys)))
               return;
            coll = sys;
         }
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
         if (sys)
            d2d::release(sys);
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

      _ptr->size = descr._size;

      // Resolve the family from the comma-separated list. Prefer the bundled
      // (custom) collection and pick the FIRST listed family it actually
      // contains. This matters for e.g. "Open Sans Condensed, Open Sans": the
      // custom collection groups the condensed faces *under* "Open Sans"
      // (selected via the stretch axis), so the first name misses and we must
      // fall through to "Open Sans" + DWRITE_FONT_STRETCH_CONDENSED rather than
      // give up and render a normal-width fallback. If none of the listed names
      // are in the custom collection, use the first against the system
      // collection (nullptr => system in CreateTextFormat).
      auto custom = d2d::custom_font_collection();
      std::wstring wfamily;
      IDWriteFontCollection* coll = nullptr;
      {
         std::istringstream str{std::string{descr._families}};
         std::string fam;
         while (std::getline(str, fam, ','))
         {
            trim(fam);
            if (fam.empty())
               continue;
            std::wstring wf = d2d::to_utf16(std::string_view{fam});
            if (wfamily.empty())
               wfamily = wf;   // first listed family = the system fallback
            if (custom)
            {
               UINT32 idx = 0;
               BOOL ex = FALSE;
               custom->FindFamilyName(wf.c_str(), &idx, &ex);
               if (ex)
               {
                  wfamily = wf;
                  coll = custom;
                  break;
               }
            }
         }
      }

      d2d::dwrite_factory()->CreateTextFormat(
         wfamily.c_str(), coll, weight, style, stretch,
         descr._size, L"en-us", &_ptr->format
      );

      resolve(coll, wfamily, weight, style, stretch, &_ptr->face);
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
