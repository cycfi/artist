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
void           draw(canvas& cnv);
std::string    get_resource_path();

#endif