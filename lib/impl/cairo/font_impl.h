#pragma once

#include <cairo.h>

namespace cycfi::artist {

struct font_impl {
    cairo_font_face_t* font_face = nullptr;
    float size = 0;
    
    font_impl()
    :font_impl(nullptr,0)
    {}
    
    font_impl(cairo_font_face_t* f, float s)
    :font_face(f)
    ,size(s)
    {}
    
    ~font_impl() {
        if( font_face ) {
            cairo_font_face_destroy(font_face);
        }
    }
};

}
