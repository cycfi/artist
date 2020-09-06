/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <string_view>

namespace cycfi::artist::unicode
{
   ////////////////////////////////////////////////////////////////////////////
   bool              is_space(char32_t cp);
   bool              is_newline(char32_t cp);
   bool              is_punctuation(char32_t cp);

   bool              is_space(char const* utf8);
   bool              is_newline(char const* utf8);
   bool              is_punctuation(char const* utf8);

   char32_t          decode_utf8(char32_t& state, char32_t& cp, char32_t byte);
   char const*       next_utf8(char const* last, char const* utf8);
   char const*       prev_utf8(char const* start, char const* utf8);
   char32_t          codepoint(char const*& utf8);
   char32_t          codepoint(char const* const& utf8);

   std::u32string    to_utf32(std::string_view s);

   ////////////////////////////////////////////////////////////////////////////
   inline bool is_space(char32_t cp)
   {
      switch (cp)
      {
         case 9:        // \t
         case 11:       // \v
         case 12:       // \f
         case 32:       // space
			case 10:		   // \n
			case 13:		   // \r
         case 0xA0:     // NBSP
            return true;
         default:
            return false;
      }
   }

   // Check if codepoint, cp, is a new line
   inline bool is_newline(char32_t cp)
   {
      switch (cp)
      {
			case 10:		   // \n
			case 13:		   // \r
			case 0x85:	   // NEL
            return true;
         default:
            return false;
      }
   }

   // Check if codepoint, cp, is a punctuation
   inline bool is_punctuation(char32_t cp)
   {
      return (cp < 0x80 && std::ispunct(cp))
         || (cp >= 0xA0 && cp <= 0xBF)
         || (cp >= 0x2000 && cp <= 0x206F)
         ;
   }

   inline bool is_space(char const* utf8)
   {
      return is_space(codepoint(utf8));
   }

   inline bool is_newline(char const* utf8)
   {
      return is_newline(codepoint(utf8));
   }

   inline bool is_punctuation(char const* utf8)
   {
      return is_newline(codepoint(utf8));
   }

   ////////////////////////////////////////////////////////////////////////////
   // Decoding UTF8
   //
   // Copyright (c) 2008-2010 Bjoern Hoehrmann <bjoern@hoehrmann.de>
   // See http://bjoern.hoehrmann.de/utf-8/decoder/dfa/ for details.
   //
   // (see codepoint and find functions below for example usage).
   ////////////////////////////////////////////////////////////////////////////

   constexpr auto utf8_accept = 0;
   constexpr auto utf8_reject = 12;

   inline char32_t decode_utf8(char32_t& state, char32_t& cp, uint8_t byte)
   {
      static constexpr uint8_t utf8d[] =
      {
         // The first part of the table maps bytes to character classes that
         // to reduce the size of the transition table and create bitmasks.
         0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,       0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
         0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,       0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
         0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,       0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
         0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,       0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
         1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,       9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
         7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,       7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
         8,8,2,2,2,2,2,2,2,2,2,2,2,2,2,2,       2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
         10,3,3,3,3,3,3,3,3,3,3,3,3,4,3,3,      11,6,6,6,5,8,8,8,8,8,8,8,8,8,8,8,

         // The second part is a transition table that maps a combination
         // of a state of the automaton and a character class to a state.
         0,12,24,36,60,96,84,12,12,12,48,72,    12,12,12,12,12,12,12,12,12,12,12,12,
         12, 0,12,12,12,12,12, 0,12, 0,12,12,   12,24,12,12,12,12,12,24,12,24,12,12,
         12,12,12,12,12,12,12,24,12,12,12,12,   12,24,12,12,12,12,12,12,12,24,12,12,
         12,12,12,12,12,12,12,36,12,36,12,12,   12,36,12,12,12,12,12,36,12,36,12,12,
         12,36,12,12,12,12,12,12,12,12,12,12,
      };

      char32_t type = utf8d[byte];

      cp = (state != utf8_accept) ?
         (byte & 0x3fu) | (cp << 6) :
         (0xff >> type) & (byte)
         ;

      state = utf8d[256 + state + type];
      return state;
   }

   ////////////////////////////////////////////////////////////////////////////
   // UTF8 Iteration. See A code point iterator adapter for C++ strings in
   // UTF-8 by Ángel José Riesgo: http://www.nubaria.com/en/blog/?p=371
   ////////////////////////////////////////////////////////////////////////////
   struct utf8_mask
   {
      static uint8_t const first    = 128;   // 1000000
      static uint8_t const second   = 64;    // 0100000
      static uint8_t const third    = 32;    // 0010000
      static uint8_t const fourth   = 16;    // 0001000
   };

   inline char const* next_utf8(char const* last, char const* utf8)
   {
      char c = *utf8;
      std::size_t offset = 1;

      if (c & utf8_mask::first)
         offset =
            (c & utf8_mask::third)?
               ((c & utf8_mask::fourth)? 4 : 3) : 2
         ;

      utf8 += offset;
      if (utf8 > last)
         utf8 = last;
      return utf8;
   }

   inline char const* prev_utf8(char const* start, char const* utf8)
   {
      if (*--utf8 & utf8_mask::first)
      {
         if ((*--utf8 & utf8_mask::second) == 0)
         {
            if ((*--utf8 & utf8_mask::second) == 0)
               --utf8;
         }
      }
      if (utf8 < start)
         utf8 = start;
      return utf8;
   }

   ////////////////////////////////////////////////////////////////////////////
   // Extracting codepoints from UTF8
   ////////////////////////////////////////////////////////////////////////////
   inline char32_t codepoint(char const*& utf8)
   {
      char32_t state = 0;
      char32_t cp;
      while (decode_utf8(state, cp, uint8_t(*utf8)))
         utf8++;
      ++utf8; // one past the last byte
      return cp;
   }

   inline char32_t codepoint(char const* const& utf8)
   {
      auto s = utf8;
      return codepoint(s);
   }

   inline std::u32string to_utf32(std::string_view s)
   {
      std::u32string s32;
      char const* last = s.data() + s.size();
      char32_t state = 0;
      char32_t cp;
      for (char const* i = s.data(); i != last; ++i)
      {
         while (decode_utf8(state, cp, uint8_t(*i)))
            i++;
         s32.push_back(cp);
      }
      std::u32string(s32).swap(s32);
      return s32;
   }
}

