/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#if !defined(ELEMENTS_TEST_APP_MARCH_12_2020)
#define ELEMENTS_TEST_APP_MARCH_12_2020

#include <canvas/canvas.hpp>
#include <string>

using namespace cycfi::elements;

int            run_app(int argc, const char* argv[]);
void           stop_app();
void           draw(canvas& cnv);

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