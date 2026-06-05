#include "test_support.hpp"

void balloon(canvas& cnv)
{
   // quadratic_curve_to
   cnv.begin_path();
   cnv.line_join(cnv.round_join);
   cnv.move_to(75, 25);
   cnv.quadratic_curve_to({25, 25}, {25, 62.5});
   cnv.quadratic_curve_to({25, 100}, {50, 100});
   cnv.quadratic_curve_to({50, 120}, {30, 125});
   cnv.quadratic_curve_to({60, 120}, {65, 100});
   cnv.quadratic_curve_to({125, 100}, {125, 62.5});
   cnv.quadratic_curve_to({125, 25}, {75, 25});
}

void heart(canvas& cnv)
{
   // bezier_curve_to
   constexpr float y0 = 40, y1 = 50, y2 = 23, y3 = 90, y4 = 45, y5 = 75, y6 = 125;
   cnv.begin_path();
   cnv.move_to({75, y0});
   cnv.bezier_curve_to({75, y4}, {70, y2}, {50, y2});
   cnv.bezier_curve_to({20, y2}, {20, y1}, {20, y1});
   cnv.bezier_curve_to({20, y5}, {y0, y3}, {75, y6});
   cnv.bezier_curve_to({110, y3}, {130, y5}, {130, y1});
   cnv.bezier_curve_to({ 130, y1}, {130, y2}, {100, y2});
   cnv.bezier_curve_to({85, y2}, {75, y4}, {75, y0});
   cnv.fill_style(color{0.2, 0, 0}.opacity(0.4));
}

void basics(canvas& cnv)
{
   auto state = cnv.new_state();

   cnv.add_rect(20, 20, 80, 40);
   cnv.fill_style(colors::navy_blue);
   cnv.fill_preserve();

   cnv.stroke_style(colors::antique_white.opacity(0.8));
   cnv.line_width(0.5);
   cnv.stroke();

   cnv.add_round_rect(40, 35, 80, 45, 8);
   cnv.fill_style(colors::light_sea_green.opacity(0.5));
   cnv.fill();

   cnv.move_to(10, 10);
   cnv.line_to(100, 100);
   cnv.line_width(10);
   cnv.stroke_style(colors::hot_pink);
   cnv.stroke();

   cnv.add_circle(120, 80, 40);
   cnv.line_width(4);
   cnv.fill_style(colors::gold.opacity(0.5));
   cnv.stroke_style(colors::gold);
   cnv.fill_preserve();
   cnv.stroke();

   cnv.translate(120, 0);
   balloon(cnv);
   cnv.stroke_style(colors::light_gray);
   cnv.stroke();

   cnv.translate(-100, 100);
   heart(cnv);
   cnv.line_width(2);
   cnv.stroke_style(color{0.8, 0, 0});
   cnv.stroke_preserve();
   cnv.fill();
}

void transformed(canvas& cnv)
{
   auto state = cnv.new_state();
   cnv.scale(1.5, 1.5);
   cnv.translate(150, 80);
   cnv.rotate(0.8);
   basics(cnv);
}

namespace detail
{
   struct corner_radii
   {
      float top_left, top_right, bottom_right, bottom_left;
      corner_radii operator+(float v) const;
      corner_radii operator-(float v) const;
   };
}

void draw_round_rect(canvas& cnv, rect bounds, detail::corner_radii radius)
{
   constexpr auto pi = 3.14159265358979323846;
   auto l = bounds.left;
   auto t = bounds.top;
   auto r = bounds.right;
   auto b = bounds.bottom;
   radius.top_left =     std::min(radius.top_left,     std::min(bounds.width(), bounds.height()) / 2);
   radius.top_right =    std::min(radius.top_right,    std::min(bounds.width(), bounds.height()) / 2);
   radius.bottom_right = std::min(radius.bottom_right, std::min(bounds.width(), bounds.height()) / 2);
   radius.bottom_left =  std::min(radius.bottom_left,  std::min(bounds.width(), bounds.height()) / 2);

   cnv.begin_path();
   cnv.arc({r-radius.bottom_right, b-radius.bottom_right}, radius.bottom_right, 0,       pi*0.5);
   cnv.arc({l+radius.bottom_left,  b-radius.bottom_left }, radius.bottom_left,  pi*0.5, pi    );
   cnv.arc({l+radius.top_left,     t+radius.top_left    }, radius.top_left,     pi,     pi*1.5);
   cnv.arc({r-radius.top_right,    t+radius.top_right   }, radius.top_right,    pi*1.5, 0      );
   cnv.close_path();
}

void basics2(canvas& cnv)
{
   auto state = cnv.new_state();
   rect bounds = {20, 20, 220, 120};

   {
      draw_round_rect(cnv, bounds, {-1, 40, 40, -1}); // Negative radius

      cnv.fill_style(colors::light_sea_green.opacity(0.8));
      cnv.fill_preserve();

      cnv.stroke_style(colors::antique_white.opacity(0.8));
      cnv.line_width(2);
      cnv.stroke();
   }
   {
      draw_round_rect(cnv, bounds.move(20, 20), {15, 40, 40, 15});

      cnv.fill_style(colors::navy_blue.opacity(0.5));
      cnv.fill_preserve();

      cnv.stroke_style(colors::antique_white.opacity(0.5));
      cnv.line_width(2);
      cnv.stroke();
   }
   {
      draw_round_rect(cnv, bounds.move(40, 40), {0, 0, 0, 0});

      cnv.fill_style(colors::indian_red.opacity(0.5));
      cnv.fill_preserve();

      cnv.stroke_style(colors::antique_white.opacity(0.5));
      cnv.line_width(2);
      cnv.stroke();
   }
}

void rainbow(canvas::gradient& gr)
{
   gr.add_color_stop(0.0/6, colors::red);
   gr.add_color_stop(1.0/6, colors::orange);
   gr.add_color_stop(2.0/6, colors::yellow);
   gr.add_color_stop(3.0/6, colors::green);
   gr.add_color_stop(4.0/6, colors::blue);
   gr.add_color_stop(5.0/6, rgb(0x4B, 0x00, 0x82));
   gr.add_color_stop(6.0/6, colors::violet);
}

void linear_gradient(canvas& cnv)
{
   auto x = 300.0f;
   auto y = 20.0f;
   auto gr = canvas::linear_gradient{x, y, x+300, y};
   rainbow(gr);

   cnv.add_round_rect(x, y, 300, 80, 5);
   cnv.fill_style(gr);
   cnv.fill();
}

void radial_gradient(canvas& cnv)
{
   auto center = point{475, 90};
   auto radius = 75.0f;
   auto gr = canvas::radial_gradient{center, 5, center.move(15, 10), radius};
   gr.add_color_stop(0.0, colors::red);
   gr.add_color_stop(1.0, colors::black);

   cnv.add_circle({center.move(15, 10), radius - 10});
   cnv.fill_style(gr);
   cnv.fill();
}

void stroke_gradient(canvas& cnv)
{
   auto x = 300.0f;
   auto y = 20.0f;
   auto gr = canvas::linear_gradient{x, y, x+300, y+80};
   gr.add_color_stop(0.0, colors::navy_blue);
   gr.add_color_stop(1.0, colors::maroon);

   cnv.add_round_rect(x, y, 300, 80, 5);
   cnv.line_width(8);
   cnv.stroke_style(gr);
   cnv.stroke();
}

void draw_pixmap(canvas& cnv)
{
   image pm{get_images_path() + "logo.png"};
   auto x = 250.0f, y = 120.0f;
   cnv.draw(pm, x, y, 0.4);
}

void line_styles(canvas& cnv)
{
   auto where = point{500, 200};
   cnv.shadow_style({5.0, 5.0}, 5, colors::black);

   cnv.stroke_style(colors::gold);
   cnv.begin_path();
   cnv.line_width(10);
   cnv.line_cap(cnv.butt);
   cnv.move_to(where.x, where.y);
   cnv.line_to(where.x+100, where.y);
   cnv.stroke();

   cnv.stroke_style(colors::sky_blue);
   cnv.begin_path();
   cnv.line_cap(cnv.round);
   cnv.move_to(where.x, where.y+20);
   cnv.line_to(where.x+100, where.y+20);
   cnv.stroke();

   cnv.stroke_style(colors::light_sea_green);
   cnv.begin_path();
   cnv.line_cap(cnv.square);
   cnv.move_to(where.x, where.y+40);
   cnv.line_to(where.x+100, where.y+40);
   cnv.stroke();

   where.x -= 20;

   cnv.stroke_style(colors::gold);
   cnv.begin_path();
   cnv.line_join(cnv.bevel_join);
   cnv.move_to(where.x, where.y+100);
   cnv.line_to(where.x+60, where.y+140);
   cnv.line_to(where.x, where.y+180);
   cnv.stroke();

   cnv.stroke_style(colors::sky_blue);
   cnv.begin_path();
   cnv.line_join(cnv.round_join);
   cnv.move_to(where.x+40, where.y+100);
   cnv.line_to(where.x+100, where.y+140);
   cnv.line_to(where.x+40, where.y+180);
   cnv.stroke();

   cnv.stroke_style(colors::light_sea_green);
   cnv.begin_path();
   cnv.line_join(cnv.miter_join);
   cnv.move_to(where.x+80, where.y+100);
   cnv.line_to(where.x+140, where.y+140);
   cnv.line_to(where.x+80, where.y+180);
   cnv.stroke();
}

void test_draw(canvas& cnv)
{
   background(cnv);
   basics(cnv);
   transformed(cnv);
   linear_gradient(cnv);
   radial_gradient(cnv);
   stroke_gradient(cnv);
   draw_pixmap(cnv);
   line_styles(cnv);
}

void test_draw2(canvas& cnv)
{
   background(cnv);
   basics2(cnv);
}

// ---------------------------------------------------------------------------
// Perceptual image comparison via CIELAB ΔE₀₀ (CIEDE2000)
//
// Cairo pixels() returns premultiplied ARGB32: uint32 = (A<<24)|(R<<16)|(G<<8)|B.
// We composite over black before conversion (premultiplied RGB over black is
// just the premultiplied values themselves), giving us the visible pixel colour.
// ---------------------------------------------------------------------------

// sRGB gamma-encoded byte → linear light [0,1]
static float srgb_to_linear(uint8_t v)
{
   float c = v / 255.0f;
   return c <= 0.04045f ? c / 12.92f
                        : std::pow((c + 0.055f) / 1.055f, 2.4f);
}

// Premultiplied ARGB32 pixel → CIELAB (L*, a*, b*)
void paths(canvas& cnv)
{
   auto stroke_fill =
      [&](path const& p, color fill_c, color stroke_c)
      {
         cnv.add_path(p);
         cnv.fill_color(fill_c);
         cnv.stroke_color(stroke_c);
         cnv.fill();
         cnv.line_width(5);
         cnv.add_path(p);
         cnv.stroke();
      };

   auto stroke =
      [&](path const& p, color stroke_c)
      {
         cnv.add_path(p);
         cnv.stroke_color(stroke_c);
         cnv.line_width(5);
         cnv.stroke();
      };

   auto dot =
      [&](float x, float y)
      {
         cnv.add_circle(x, y, 10);
         cnv.fill_color(colors::white.opacity(0.5));
         cnv.fill();
      };

   background(cnv);

   // These paths are defined using SVG-style strings lifted off the
   // W3C SVG documentation: https://www.w3.org/TR/SVG/paths.html

   {
      auto save = cnv.new_state();
      cnv.scale(0.5, 0.5);

      cnv.translate(-40, 0);
      path p1{"M 100 100 L 300 100 L 200 300 z"};
      stroke_fill(p1, colors::green.opacity(0.5), colors::ivory);

      cnv.translate(220, 0);
      path p2{"M100,200 C100,100 250,100 250,200 S400,300 400,200"};
      stroke(p2, colors::light_sky_blue);

      cnv.translate(-150, 250);
      path p3{"M200,300 Q400,50 600,300 T1000,300"};
      stroke(p3, colors::light_sky_blue);

      path p4{"M200,300 L400,50 L600,300 L800,550 L1000,300"};
      stroke(p4, colors::light_gray.opacity(0.5));
      dot(200, 300);
      dot(600, 300);
      dot(1000, 300);
      dot(400, 50);
      dot(800, 550);
      cnv.translate(150, -250);

      cnv.translate(350, 0);
      path p5{"M300,200 h-150 a150,150 0 1,0 150,-150 z"};
      stroke_fill(p5, colors::red.opacity(0.8), colors::ivory);

      path p6{"M275,175 v-150 a150,150 0 0,0 -150,150 z"};
      stroke_fill(p6, colors::blue.opacity(0.8), colors::ivory);

      cnv.translate(-350, 200);
      path p7{
         "M600,350 l 50,-25"
         "a25,25 -30 0,1 50,-25 l 50,-25"
         "a25,50 -30 0,1 50,-25 l 50,-25"
         "a25,75 -30 0,1 50,-25 l 50,-25"
         "a25,100 -30 0,1 50,-25 l 50,-25"
      };
      stroke(p7, colors::ivory);
   }
}

void compare_transform(affine_transform const& a, affine_transform const& b)
{
   CHECK(a.a == Approx(b.a));
   CHECK(a.b == Approx(b.b));
   CHECK(a.c == Approx(b.c));
   CHECK(a.d == Approx(b.d));
   CHECK(a.tx == Approx(b.tx));
   CHECK(a.ty == Approx(b.ty));
}

void misc(canvas& cnv)
{
   using cycfi::pi;

   background(cnv);
   cnv.clear_rect(30, 30, 50, 50);

   {
      auto save = cnv.new_state();

      affine_transform mat = cnv.transform();
      cnv.fill_style(colors::blue);

      cnv.rotate(pi/4);
      mat = mat.rotate(pi/4);
      auto ctm = cnv.transform();
      compare_transform(mat, ctm);

      cnv.scale(2);
      mat = mat.scale(2);
      ctm = cnv.transform();
      compare_transform(mat, ctm);

      cnv.translate(100, 0);
      mat = mat.translate(100, 0);
      ctm = cnv.transform();
      compare_transform(mat, ctm);

      cnv.add_rect(-25, -25, 50, 50);
      cnv.fill();

      cnv.transform(mat);
      cnv.add_rect(-20, -20, 40, 40);
      cnv.fill_style(colors::red);
      cnv.fill();
   }

   {
      affine_transform mat;
      auto p = mat.apply(0.0, 0.0);
      CHECK(p.x == Approx(0));
      CHECK(p.y == Approx(0));

      mat = mat.scale(10);
      p = mat.apply(4, 4);
      CHECK(p.x == Approx(40));
      CHECK(p.y == Approx(40));

      mat = mat.translate(2, 2);
      p = mat.apply(0.0, 0.0);
      CHECK(p.x == Approx(20));
      CHECK(p.y == Approx(20));
   }

   {
      auto save = cnv.new_state();

      affine_transform mat = cnv.transform();
      mat = mat.translate(144, 144);
      mat = mat.skew(pi/8, pi/12);
      cnv.transform(mat);
      cnv.add_rect(0, 0, 72, 72);
      cnv.fill_style(colors::green);
      cnv.fill();
   }

   {
      path p;
      p.add_circle(circle{230, 230, 50});
      p.add_circle(circle{230, 230, 25});
      p.fill_rule(path::fill_odd_even);

      CHECK(p.includes(230-50+5, 230));
      CHECK(!p.includes(230, 230));

      cnv.clip(p);
      cnv.add_rect(0, 0, 500, 500);
      cnv.fill_style(colors::navajo_white.opacity(0.5));
      cnv.fill_preserve();

      CHECK(cnv.point_in_path(10, 10));
      CHECK(!p.includes(501, 500));
   }

   // Test small offscreen hit testing and text measurements work
   {
      image img{1, 1};
      offscreen_image offscr{img};
      canvas cnv{offscr.context()};

      cnv.add_circle(230, 230, 50);
      cnv.add_circle(230, 230, 25);
      cnv.fill_rule(path::fill_odd_even);

      CHECK(cnv.point_in_path(230-50+5, 230));
      CHECK(!cnv.point_in_path(230, 230));

      cnv.font(font_descr{"Open Sans", 36});
      auto m = cnv.measure_text("Hello, World");
      CHECK(std::abs(m.size.x-205.0f) <= 1.0);
      CHECK(std::abs(m.size.y-49) <= 1.0);
      CHECK(std::floor(m.ascent) == 38);
      CHECK(std::floor(m.descent) == 10);
      CHECK(std::floor(m.leading) == 0);
   }
}
void chessboard(canvas& cnv)
{
   int constexpr rows = 8;
   int constexpr cols = 8;
   int constexpr square_side = 60;
   int constexpr square_area = square_side * square_side;
   size_t constexpr pix_buf_size = rows * cols * square_area;

   uint32_t constexpr white = 0xffffffff;
   auto black = []() -> uint32_t { return cycfi::is_little_endian() ? 0xff000000 : 0x000000ff; };

   std::unique_ptr<uint32_t[]> pix_buf(new uint32_t[pix_buf_size]);
   for (int y = 0; y < rows; y++)
   {
      uint32_t* row_slice = &pix_buf[y * cols * square_area];
      uint32_t color = (y % 2 != 0) ? white : black();
      for (int x = 0; x < cols * square_area; x++)
      {
         if (x % square_side == 0)
            color = (color == white) ? black() : white;
         row_slice[x] = color;
      }
   }

   auto img = make_image<pixel_format::rgba32>(
      pix_buf.get(), {float(cols * square_side), float(rows * square_side)});
   cnv.draw(img, {0, 0});
}

TEST_CASE("Drawing")
{
   image pm{window_size};
   {
      offscreen_image ctx{pm};
      canvas pm_cnv{ctx.context()};
      test_draw(pm_cnv);
   }
   compare_golden(pm, "shapes_and_images");
}
TEST_CASE("Drawing2")
{
   image pm{window_size};
   {
      offscreen_image ctx{pm};
      canvas pm_cnv{ctx.context()};
      test_draw2(pm_cnv);
   }
   compare_golden(pm, "shapes2");
}
TEST_CASE("Paths")
{
   image pm{window_size};
   {
      offscreen_image ctx{pm};
      canvas pm_cnv{ctx.context()};
      paths(pm_cnv);
   }
   compare_golden(pm, "paths");
}
TEST_CASE("Misc")
{
   image pm{window_size};
   {
      offscreen_image ctx{pm};
      canvas pm_cnv{ctx.context()};
      misc(pm_cnv);
   }
   compare_golden(pm, "misc");
}

TEST_CASE("Chessboard")
{
   image pm{window_size};
   {
      offscreen_image ctx{pm};
      canvas pm_cnv{ctx.context()};
      chessboard(pm_cnv);
   }
   compare_golden(pm, "chessboard");
}
