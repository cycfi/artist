/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include "app.hpp"

using namespace cycfi::artist;
auto constexpr window_size = point{ 640.0f, 480.0f };

void draw(canvas& cnv)
{
    // Typically, you don't want to do this when drawing, but
    // we do it here for the sake of easy demonstration.
    static auto space = std::make_shared<picture>("space.jpg");

    cnv.draw(*space);
}

int main(int argc, const char* argv[])
{
    return run_app(argc, argv, window_size);
}

