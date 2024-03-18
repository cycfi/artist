/*=============================================================================
   Copyright (c) 2021 Chance Snow, Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#ifndef __ARTIST_STRINGS_H
#define __ARTIST_STRINGS_H

#ifdef __cplusplus
extern "C" {
#endif

   typedef struct string_view string_view;

   string_view*   artist_string_view_from_utf8(const char* str);
   void           artist_string_view_destroy(string_view* str);

#ifdef __cplusplus
}
#endif
#endif
