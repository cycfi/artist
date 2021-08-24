#ifndef __ARTIST_STRINGS_H
#define __ARTIST_STRINGS_H

#ifdef __cplusplus
extern "C" {
#endif

   typedef struct std::string_view string_view;

   string_view*   artist_string_view_from_utf8(const char* str) {
      // string_view* result = (string_view*) malloc(sizeof(std::strlen));
      // *result = std::basic_string(str);
      return new std::string_view(str);
   }
   void           artist_string_view_destroy(string_view* str) { delete str; }

#ifdef __cplusplus
}
#endif
#endif
