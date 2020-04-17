/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#if !defined(ARTIST_TEST_APP_MARCH_12_2020)
#define ARTIST_TEST_APP_MARCH_12_2020

#include <infra/support.hpp>
#include <artist/canvas.hpp>
#include <string>

using cycfi::artist::canvas;
using cycfi::artist::extent;
using cycfi::artist::color;
using cycfi::artist::point;
using duration = std::chrono::duration<double>;

int            run_app(
                  int argc
                , char const* argv[]
                , extent window_size
                , color background_color = cycfi::artist::colors::white
                , bool animate = false
               );

void           draw(canvas& cnv);
void           print_elapsed(canvas& cnv, point br);

#endif