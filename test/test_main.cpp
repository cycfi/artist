// UNICODE must be defined before including Catch: on MSVC the Unicode charset
// defines _UNICODE, which makes Catch emit a wmain entry point that calls its
// wchar_t applyCommandLine overload -- and that overload is only declared when
// UNICODE is defined. (The former single main.cpp set this before catch; the
// split must do the same here.)
#if defined(_WIN32)
# ifndef UNICODE
#  define UNICODE
# endif
#endif

#define CATCH_CONFIG_MAIN
#include <infra/catch.hpp>
#include "test_support.hpp"

#if defined(ARTIST_QUARTZ_2D)
   // Initialize the fonts and resources
   struct resource_setter
   {
      resource_setter();
   };
   static resource_setter init_resources;
#endif

namespace cycfi::artist
{
   void init_paths()
   {}

   // This is declared in font.hpp
   fs::path get_user_fonts_directory()
   {
      return get_fonts_path();
   }
}
