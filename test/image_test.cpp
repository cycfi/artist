/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]

   Asserts the claims of docs/modules/ROOT/pages/foundation/image.adoc. Each
   case names the page section it comes from. Image behaviour is uniform
   across the Quartz 2D, Skia and Cairo backends: the rendered pixels differ
   (each rasterizer anti-aliases differently) but the behaviour does not.
=============================================================================*/
#include "test_support.hpp"
#include <fstream>
#include <type_traits>

namespace fs = cycfi::fs;

namespace
{
   struct rgba8
   {
      int r, g, b, a;
   };

   bool near(rgba8 x, rgba8 y, int tol = 2)
   {
      return std::abs(x.r - y.r) <= tol && std::abs(x.g - y.g) <= tol
         && std::abs(x.b - y.b) <= tol && std::abs(x.a - y.a) <= tol;
   }

   // The channels of pixel (x, y): pixels() is premultiplied B, G, R, A on
   // every backend.
   rgba8 pixel_at(image const& img, int x, int y)
   {
      auto p = reinterpret_cast<std::uint8_t const*>(img.pixels());
      p += 4 * (y * int(img.bitmap_size().x) + x);
      return {p[2], p[1], p[0], p[3]};
   }

   void fill(image& img, color c, rect r)
   {
      offscreen_image ctx{img};
      canvas cnv{ctx.context()};
      cnv.fill_style(c);
      cnv.fill_rect(r);
   }

   // Draw src into a blank image the same size and return pixel (0, 0).
   rgba8 drawn(image const& src)
   {
      image dst{src.size()};
      {
         offscreen_image ctx{dst};
         canvas cnv{ctx.context()};
         cnv.draw(src, point{0, 0});
      }
      return pixel_at(dst, 0, 0);
   }

   void png_dims(std::string const& path, std::uint32_t& w, std::uint32_t& h)
   {
      std::ifstream f{path, std::ios::binary};
      unsigned char b[24] = {};
      f.read(reinterpret_cast<char*>(b), 24);
      w = (b[16] << 24) | (b[17] << 16) | (b[18] << 8) | b[19];
      h = (b[20] << 24) | (b[21] << 16) | (b[22] << 8) | b[23];
   }

   auto const missing_file = fs::path{"/nonexistent_dir_artist_test/a.png"};
   auto const unwritable_file = std::string{"/nonexistent_dir_artist_test/a.png"};

   rgba8 const red = {255, 0, 0, 255};
   rgba8 const blue = {0, 0, 255, 255};
}

TEST_CASE("Image: Constructors and Assignment", "[image]")
{
   static_assert(!std::is_copy_constructible_v<image>);
   static_assert(!std::is_copy_assignable_v<image>);
   static_assert(std::is_nothrow_move_constructible_v<image>);
   static_assert(std::is_nothrow_move_assignable_v<image>);
   static_assert(!std::is_convertible_v<extent, image>);
   static_assert(!std::is_convertible_v<fs::path, image>);
   static_assert(std::is_same_v<image_ptr, std::shared_ptr<image>>);

   CHECK(image(10, 20).size() == extent{10, 20});
   CHECK(image(extent{10, 20}).size() == extent{10, 20});

   // Blank image storage (Backend Differences).
   {
      image img{10, 20};
      REQUIRE(img.pixels() != nullptr);
      CHECK(img.bitmap_size() == extent{10, 20});
      for (int i = 0; i != 10 * 20; ++i)
         CHECK(img.pixels()[i] == 0);
   }

   {
      image img{fs::path{get_images_path() + "logo.png"}};
      CHECK(img.size() == extent{512, 512});
   }

   CHECK_THROWS_AS(image{missing_file}, std::runtime_error);

   {
      image a{10, 20};
      image b{std::move(a)};
      CHECK(b.size() == extent{10, 20});

      image c{3, 3};
      c = std::move(b);
      CHECK(c.size() == extent{10, 20});
   }
}

TEST_CASE("Image: Construction from Pixels", "[image]")
{
   // Opaque pixels read the same on every backend.
   {
      std::uint8_t buf[4] = {0x80, 0x80, 0x80, 0x80};
      auto img = make_image<pixel_format::gray8>(buf, {2, 2});
      CHECK(img.size() == extent{2, 2});
      CHECK(near(drawn(img), {0x80, 0x80, 0x80, 255}));
   }
   {
      // Bytes R, G, B, A in memory.
      std::uint8_t buf[16] = {
         255, 0, 0, 255,  255, 0, 0, 255,  255, 0, 0, 255,  255, 0, 0, 255
      };
      auto img = make_image<pixel_format::rgba32>(
         reinterpret_cast<std::uint32_t const*>(buf), {2, 2});
      CHECK(img.size() == extent{2, 2});
      CHECK(near(drawn(img), red));
   }

   {
      // RGB 5-6-5, red in the top five bits.
      std::uint16_t buf[4] = {0xF800, 0xF800, 0xF800, 0xF800};
      auto img = make_image<pixel_format::rgb16>(buf, {2, 2});
      CHECK(near(drawn(img), red));
   }
   {
      // Bytes R, G, B and an ignored fourth byte in memory.
      std::uint8_t buf[16] = {
         255, 0, 0, 0,  255, 0, 0, 0,  255, 0, 0, 0,  255, 0, 0, 0
      };
      auto img = make_image<pixel_format::rgb32>(
         reinterpret_cast<std::uint32_t const*>(buf), {2, 2});
      CHECK(near(drawn(img), red));
   }
}

TEST_CASE("Image: Accessors and Pixel Access", "[image]")
{
   // Row major, bitmap_size().x pixels to a row, no padding: a padded row
   // would put (0, 7) somewhere other than 7 * 10.
   {
      image img{10, 10};
      {
         offscreen_image ctx{img};
         canvas cnv{ctx.context()};
         cnv.fill_style(colors::red);
         cnv.fill_rect(0, 0, 10, 10);
         cnv.fill_style(colors::blue);
         cnv.fill_rect(0, 0, 10, 5);
      }
      CHECK(img.size() == extent{10, 10});
      CHECK(near(pixel_at(img, 9, 2), blue));
      CHECK(near(pixel_at(img, 0, 7), red));
      CHECK(near(pixel_at(img, 9, 9), red));

      // Premultiplied B, G, R, A bytes, and a bitmap the same size as the
      // image.
      CHECK(img.bitmap_size() == img.size());
      auto p = reinterpret_cast<std::uint8_t const*>(img.pixels());
      CHECK(p[0] == 255);
      CHECK(p[1] == 0);
      CHECK(p[2] == 0);
      CHECK(p[3] == 255);
   }

   // A loaded PNG: premultiplied B, G, R, A on every backend. logo.png is
   // transparent at (0, 0) and opaque (0x2e, 0x27, 0x6c) at (256, 256).
   {
      image logo{fs::path{get_images_path() + "logo.png"}};
      auto p = reinterpret_cast<std::uint8_t const*>(logo.pixels());
      REQUIRE(p != nullptr);
      auto w = int(logo.bitmap_size().x);
      auto q = p + 4 * (256 * w + 256);
      CHECK(p[3] == 0);
      CHECK(near({q[2], q[1], q[0], q[3]}, {0x2e, 0x27, 0x6c, 255}));
      // Premultiplied: a transparent pixel has no colour left.
      CHECK((p[0] | p[1] | p[2]) == 0);
   }

   // A make_image image: converted to premultiplied B, G, R, A on every
   // backend. gray8 0x10 becomes opaque gray.
   {
      std::uint8_t buf[4] = {0x10, 0x20, 0x30, 0x40};
      auto img = make_image<pixel_format::gray8>(buf, {2, 2});
      auto p = reinterpret_cast<std::uint8_t const*>(img.pixels());
      REQUIRE(p != nullptr);
      CHECK(p[0] == 0x10);
      CHECK(p[1] == 0x10);
      CHECK(p[2] == 0x10);
      CHECK(p[3] == 0xff);
      CHECK(p[4] == 0x20);
   }
}

TEST_CASE("Image: Output", "[image]")
{
   // The PNG is size().x by size().y pixels.
   image img{10, 20};
   fill(img, colors::red, {0, 0, 10, 20});

   auto path = get_results_path() + "image_test_output.png";
   img.save_png(path);

   std::uint32_t w = 0, h = 0;
   png_dims(path, w, h);
   CHECK(w == 10);
   CHECK(h == 20);

   image loaded{fs::path{path}};
   CHECK(loaded.size() == extent{10, 20});
}

TEST_CASE("Image: Offscreen Drawing", "[image]")
{
   static_assert(!std::is_copy_constructible_v<offscreen_image>);
   static_assert(!std::is_move_constructible_v<offscreen_image>);
   static_assert(!std::is_copy_assignable_v<offscreen_image>);

   // The drawing is in the image once the offscreen_image is destroyed.
   image img{10, 10};
   fill(img, colors::red, {0, 0, 10, 10});
   CHECK(near(pixel_at(img, 5, 5), red));

   // Every backend draws over what the image already holds.
   fill(img, colors::blue, {0, 0, 5, 10});
   CHECK(near(pixel_at(img, 1, 5), blue));
   CHECK(near(pixel_at(img, 8, 5), red));
}

TEST_CASE("Image: save_png throws on failure", "[image]")
{
   image img{4, 4};
   fill(img, colors::red, {0, 0, 4, 4});
   CHECK_THROWS_AS(img.save_png(unwritable_file), std::runtime_error);
}

TEST_CASE("Image: make_image copies the buffer", "[image]")
{
   // Overwrite the buffer after make_image and before the image is used;
   // the copy is unaffected.
   std::uint8_t buf[16] = {
      255, 0, 0, 255,  255, 0, 0, 255,  255, 0, 0, 255,  255, 0, 0, 255
   };
   auto img = make_image<pixel_format::rgba32>(
      reinterpret_cast<std::uint32_t const*>(buf), {2, 2});
   for (int i = 0; i != 4; ++i)
   {
      buf[i * 4] = 0;
      buf[i * 4 + 2] = 255;
   }
   CHECK(near(drawn(img), red));
}

TEST_CASE("Image: rgba32 is straight alpha, premultiplied in", "[image]")
{
   // R = 0x80, A = 0x80 straight in becomes premultiplied 0x40 red at half
   // opacity; pixel_at reports the premultiplied bytes.
   std::uint8_t buf[16] = {
      0x80, 0, 0, 0x80,  0x80, 0, 0, 0x80,  0x80, 0, 0, 0x80,  0x80, 0, 0, 0x80
   };
   auto img = make_image<pixel_format::rgba32>(
      reinterpret_cast<std::uint32_t const*>(buf), {2, 2});
   CHECK(near(drawn(img), {0x40, 0, 0, 0x80}));
}

TEST_CASE("Image: offscreen drawing reaches the image immediately", "[image]")
{
   // Save the image while the offscreen_image is still alive; the drawing is
   // already there on every backend.
   auto path = get_results_path() + "image_test_during.png";
   image img{10, 10};
   {
      offscreen_image ctx{img};
      canvas cnv{ctx.context()};
      cnv.fill_style(colors::red);
      cnv.fill_rect(0, 0, 10, 10);
      img.save_png(path);
   }
   image during{fs::path{path}};
   auto p = reinterpret_cast<std::uint8_t const*>(during.pixels());
   REQUIRE(p != nullptr);
   p += 4 * (5 * int(during.bitmap_size().x) + 5);
   CHECK(p[3] == 255);
   CHECK(near(pixel_at(img, 5, 5), red));
}

TEST_CASE("Image: draw works in units at any scale", "[image]")
{
   // Page, Accessors: draw places the image at size() in user space, and the
   // src rectangle is in the same units, whatever the image's scale. The
   // source is 4 by 2 units at scale 2 (8 by 4 pixels): red on the left half,
   // blue on the right.
   image src{4, 2, 2};
   fill(src, colors::red, {0, 0, 2, 2});
   fill(src, colors::blue, {2, 0, 4, 2});

   // The whole image, at its size.
   {
      image dst{4, 2};
      {
         offscreen_image ctx{dst};
         canvas cnv{ctx.context()};
         cnv.draw(src, point{0, 0});
      }
      CHECK(near(pixel_at(dst, 0, 1), red));
      CHECK(near(pixel_at(dst, 3, 1), blue));
   }

   // A src rectangle in units: the right half, stretched over the whole
   // destination.
   {
      image dst{4, 2};
      {
         offscreen_image ctx{dst};
         canvas cnv{ctx.context()};
         cnv.draw(src, rect{2, 0, 4, 2}, rect{0, 0, 4, 2});
      }
      CHECK(near(pixel_at(dst, 0, 1), blue));
      CHECK(near(pixel_at(dst, 3, 1), blue));
   }
}

// Page, Overview: the file formats read on every backend. Both test files are
// 32 by 16 with red (200, 40, 40) on the left half. formats.jpg is blue
// (40, 40, 200) on the right; formats.webp is lossless and transparent on the
// right. One case per format, so a failing format does not hide another.
TEST_CASE("Image: loads JPEG", "[image]")
{
   image img{fs::path{get_images_path() + "formats.jpg"}};
   CHECK(img.size() == extent{32, 16});
   CHECK(near(pixel_at(img, 8, 8), {200, 40, 40, 255}, 12));
   CHECK(near(pixel_at(img, 24, 8), {40, 40, 200, 255}, 12));
}

TEST_CASE("Image: loads WebP", "[image]")
{
   image img{fs::path{get_images_path() + "formats.webp"}};
   CHECK(img.size() == extent{32, 16});
   CHECK(near(pixel_at(img, 8, 8), {200, 40, 40, 255}));
   CHECK(near(pixel_at(img, 24, 8), {0, 0, 0, 0}));
}