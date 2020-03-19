/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#if !defined(ELEMENTS_TEST_APP_MARCH_12_2020)
#define ELEMENTS_TEST_APP_MARCH_12_2020

#include <artist/canvas.hpp>
#include <string>

using cycfi::artist::canvas;
using cycfi::artist::extent;

int            run_app(
                  int argc
                , const char* argv[]
                , extent window_size
                , bool animate = false
               );

void           stop_app();
void           draw(canvas& cnv);

#endif