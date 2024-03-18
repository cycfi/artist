#include <artist/c/canvas.h>

void stroke_fill(canvas* cnv, path* p, color fill_c, color stroke_c) {
   artist_canvas_add_path(cnv, p);
   artist_canvas_fill_color(cnv, fill_c);
   artist_canvas_stroke_color(cnv, stroke_c);
   artist_canvas_fill(cnv);
   artist_canvas_line_width(cnv, 5);
   artist_canvas_add_path(cnv, p);
   artist_canvas_stroke(cnv);
}

void stroke(canvas* cnv, path* p, color stroke_c) {
   artist_canvas_add_path(cnv, p);
   artist_canvas_stroke_color(cnv, stroke_c);
   artist_canvas_line_width(cnv, 5);
   artist_canvas_stroke(cnv);
}

void dot(canvas* cnv, float x, float y) {
   artist_canvas_add_circle(cnv, x, y, 10);
   artist_canvas_fill_color(cnv, artist_color_opacity(COLOR_WHITE, 0.5));
   artist_canvas_fill(cnv);
}

int main(int argc, char const *argv[])
{
   image* buffer = artist_image_create(400, 300);
   offscreen_image* surface = artist_offscreen_image_create(buffer);
   canvas* cnv = artist_offscreen_image_context(surface);

   // These paths are defined using SVG-style strings lifted off the
   // W3C SVG documentation: https://www.w3.org/TR/SVG/paths.html

   // TODO: auto save = cnv.new_state();
   artist_canvas_scale(cnv, 0.5, 0.5);

   artist_canvas_translate(cnv, -40, 0);
   path* p1 = artist_path_create_from_svg("M 100 100 L 300 100 L 200 300 z");
   stroke_fill(cnv, p1, artist_color_opacity(COLOR_GREEN, 0.5), COLOR_IVORY);

   artist_canvas_translate(cnv, 220, 0);
   path* p2 = artist_path_create_from_svg("M100,200 C100,100 250,100 250,200 S400,300 400,200");
   stroke(cnv, p2, COLOR_LIGHT_SKY_BLUE);

   artist_canvas_translate(cnv, -150, 250);
   path* p3 = artist_path_create_from_svg("M200,300 Q400,50 600,300 T1000,300");
   stroke(cnv, p3, COLOR_LIGHT_SKY_BLUE);

   path* p4 = artist_path_create_from_svg("M200,300 L400,50 L600,300 L800,550 L1000,300");
   stroke(cnv, p4, artist_color_opacity(COLOR_LIGHT_GRAY, 0.5));
   dot(cnv, 200, 300);
   dot(cnv, 600, 300);
   dot(cnv, 1000, 300);
   dot(cnv, 400, 50);
   dot(cnv, 800, 550);
   artist_canvas_translate(cnv, 150, -250);

   artist_canvas_translate(cnv, 350, 0);
   path* p5 = artist_path_create_from_svg("M300,200 h-150 a150,150 0 1,0 150,-150 z");
   stroke_fill(cnv, p5, artist_color_opacity(COLOR_RED, 0.8), COLOR_IVORY);

   path* p6 = artist_path_create_from_svg("M275,175 v-150 a150,150 0 0,0 -150,150 z");
   stroke_fill(cnv, p6, artist_color_opacity(COLOR_BLUE, 0.8), COLOR_IVORY);

   artist_canvas_translate(cnv, -350, 200);
   path* p7 = artist_path_create_from_svg(
      "M600,350 l 50,-25"
      "a25,25 -30 0,1 50,-25 l 50,-25"
      "a25,50 -30 0,1 50,-25 l 50,-25"
      "a25,75 -30 0,1 50,-25 l 50,-25"
      "a25,100 -30 0,1 50,-25 l 50,-25"
   );
   stroke(p7, COLOR_IVORY);

   artist_image_save_png(buffer, create);

   return 0;
}
