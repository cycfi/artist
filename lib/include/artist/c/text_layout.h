/*=============================================================================
   Copyright (c) 2021 Chance Snow, Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#ifndef __ARTIST_TEXT_LAYOUT_H
#define __ARTIST_TEXT_LAYOUT_H

#include <artist/text_layout.hpp>

#include "canvas.h"
#include "color.h"
#include "font.h"
#include "point.h"
#include "strings.h"

#ifdef __cplusplus
using namespace cycfi;
extern "C" {
#endif

   typedef struct artist::text_layout text_layout;

   text_layout*            artist_text_layout_create(font_descr font_, string_view* utf8);
   text_layout*            artist_text_layout_create_utf32(font_descr font_, std::u32string_view* utf32);
   void                    artist_text_layout_destroy(text_layout* layout);

   void                    artist_text_layout_set_text(text_layout* layout, string_view* utf8);
   void                    artist_text_layout_set_text_utf32(text_layout* layout, std::u32string_view* utf32);
   std::u32string_view     artist_text_layout_text(text_layout* layout);

   typedef struct artist::text_layout::line_info line_info;
   typedef struct artist::text_layout::flow_info flow_info;
   typedef artist::text_layout::get_line_info get_line_info;

   void                    artist_text_layout_flow(text_layout* layout, float width, bool justify = false);
   void                    artist_text_layout_flow_line_info(text_layout* layout, get_line_info const glf, flow_info finfo);
   void                    artist_text_layout_draw_pt(text_layout* layout, canvas* cnv, point p, color c = colors::black);
   void                    artist_text_layout_draw(text_layout* layout, canvas* cnv, float x, float y, color c = colors::black);

   typedef artist::text_layout::break_enum break_enum;

   std::size_t             artist_text_layout_num_lines(text_layout* layout);
   point                   artist_text_layout_caret_point(text_layout* layout, std::size_t index);
   std::size_t             artist_text_layout_caret_index(text_layout* layout, float x, float y);
   std::size_t             artist_text_layout_caret_index_pt(text_layout* layout, point p);
   break_enum              artist_text_layout_line_break(text_layout* layout, std::size_t index);
   break_enum              artist_text_layout_word_break(text_layout* layout, std::size_t index);

#ifdef __cplusplus
}
#endif
#endif
