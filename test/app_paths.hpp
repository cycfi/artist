/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#if !defined(ARTIST_TEST_PATHS_MARCH_12_2020)
#define ARTIST_TEST_PATHS_MARCH_12_2020

#include <artist/canvas.hpp>
#include <string>

inline std::string get_fonts_path()
{
   return FONTS_PATH;
}

inline std::string get_images_path()
{
   return IMAGES_PATH;
}

inline std::string get_golden_path()
{
   return GOLDEN_PATH;
}

inline std::string get_results_path()
{
   return RESULTS_PATH;
}

#endif