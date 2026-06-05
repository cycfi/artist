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
