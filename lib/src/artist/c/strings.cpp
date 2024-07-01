/*=============================================================================
   Copyright (c) 2021 Chance Snow, Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <artist/c/strings.h>

extern "C" {
   string_view*   artist_string_view_from_utf8(const char* str) {
      // string_view* result = (string_view*) malloc(sizeof(std::strlen));
      // *result = std::basic_string(str);
      return new std::string_view(str);
   }
   void           artist_string_view_destroy(string_view* str) { delete str; }
}
