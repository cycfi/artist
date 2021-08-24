/*=============================================================================
   Copyright (c) 2021 Chance Snow, Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <artist/c/text_layout.h>

using namespace cycfi;

extern "C" {
   text_layout*            artist_text_layout_create(font_descr font_, string_view* utf8) { return new artist::text_layout(font_, *utf8); }
   text_layout*            artist_text_layout_create_utf32(font_descr font_, std::u32string_view* utf32) { return new artist::text_layout(font_, *utf32); }
   void                    artist_text_layout_destroy(text_layout* layout) { delete layout; }

   void                    artist_text_layout_set_text(text_layout* layout, string_view* utf8) { layout->text(*utf8); }
   void                    artist_text_layout_set_text_utf32(text_layout* layout, std::u32string_view* utf32) { layout->text(*utf32); }
   std::u32string_view     artist_text_layout_text(text_layout* layout) { return layout->text(); }

   void                    artist_text_layout_flow(text_layout* layout, float width, bool justify) { return layout->flow(width, justify); }
   void                    artist_text_layout_flow_line_info(text_layout* layout, get_line_info const glf, flow_info finfo) { return layout->flow(glf, finfo); }
   void                    artist_text_layout_draw_pt(text_layout* layout, canvas* cnv, point p, color c) { return layout->draw(*cnv, p, c); }
   void                    artist_text_layout_draw(text_layout* layout, canvas* cnv, float x, float y, color c) { return layout->draw(*cnv, x, y, c); }

   std::size_t             artist_text_layout_num_lines(text_layout* layout) { return layout->num_lines(); }
   point                   artist_text_layout_caret_point(text_layout* layout, std::size_t index) { return layout->caret_point(index); }
   std::size_t             artist_text_layout_caret_index(text_layout* layout, float x, float y) { return layout->caret_index(x, y); }
   std::size_t             artist_text_layout_caret_index_pt(text_layout* layout, point p) { return layout->caret_index(p); }
   break_enum              artist_text_layout_line_break(text_layout* layout, std::size_t index) { return layout->line_break(index); }
   break_enum              artist_text_layout_word_break(text_layout* layout, std::size_t index) { return layout->word_break(index); }
}
