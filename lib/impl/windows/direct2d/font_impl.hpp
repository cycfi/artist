/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]

   Shared DirectWrite font backing, read by font.cpp, canvas.cpp (fill_text /
   measure_text) and text_run.cpp. The IDWriteTextFormat and IDWriteFontFace
   are device-independent (created from the shared DWrite factory).
=============================================================================*/
#if !defined(ARTIST_D2D_FONT_IMPL_JUNE_7_2026)
#define ARTIST_D2D_FONT_IMPL_JUNE_7_2026

#include "context.hpp"
#include <string>
#include <string_view>

namespace cycfi::artist
{
   struct font_impl
   {
      IDWriteTextFormat*   format  = nullptr;   // for layout/draw
      IDWriteFontFace*     face    = nullptr;   // for metrics + glyph outlines
      float                size    = 0;
      float                ascent  = 0;
      float                descent = 0;
      float                leading = 0;
   };

   namespace d2d
   {
      // utf8/utf32 -> utf16 (DirectWrite speaks UTF-16). Defined in font.cpp.
      std::wstring to_utf16(std::string_view utf8);
      std::wstring to_utf16(std::u32string_view utf32);
   }
}

#endif
