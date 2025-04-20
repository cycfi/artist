/*=============================================================================
   Copyright (c) 2016-2023 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include "../../app.hpp"

#include <GL/gl.h>

#include "SkCanvas.h"
#include <chrono>
#include <iostream>

#include "application.h"

using namespace cycfi::artist;
float elapsed_ = 0;  // rendering elapsed time

void render(SkCanvas* gpu_canvas, float scale)
{
    auto start = std::chrono::steady_clock::now();

    gpu_canvas->save();
    gpu_canvas->scale(scale, scale);
    auto cnv = canvas{gpu_canvas};
    draw(cnv);
    gpu_canvas->restore();

    auto stop = std::chrono::steady_clock::now();
    elapsed_ = std::chrono::duration<double>{stop - start}.count();
}

namespace cycfi::artist
{
   void init_paths()
   {
      add_search_path(fs::current_path() / "resources");
      add_search_path(fs::current_path() / "resources/fonts");
      add_search_path(fs::current_path() / "resources/images");
   }

   // This is declared in font.hpp
   fs::path get_user_fonts_directory()
   {
      return fs::path(fs::current_path() / "resources/fonts");
   }
}

class Window: public Surface
{
public:
    using Surface::Surface;

    std::function<void()> onClosed;
    //std::function<void(SkCanvas *surf, float scale)> onDraw;
    std::function<void(int,int)> onReshape;

    bool animate;

private:
    //bool m_opaque;

    bool draw(float scale) override
    {
        render(skia_surf->getCanvas(), scale);
        return animate;
    }

    void closed() override
    {if (onClosed) onClosed();}

    void configure(uint32_t width, uint32_t height, unsigned state) override
    {
        onReshape(width, height);
    }
};

int run_app(
   int argc
 , char const* argv[]
 , extent window_size
 , color background_color
 , bool animate
)
{
   try {

        Application app;

        Window window(window_size.x, window_size.y);

        window.setTitle("Example application");
        window.animate = animate;
        window.onClosed = [&app](){app.stop();};
        window.onReshape = [&background_color](int, int) {
            glClearColor(background_color.red + 0.1,
                         background_color.green,
                         background_color.blue,
                         background_color.alpha);
            glClear(GL_COLOR_BUFFER_BIT);
        };

        app.run();

    } catch (const char* err) {
        std::cerr<<"Error: "<<err<<std::endl;
        exit(1);
    }

    return 0;
}


