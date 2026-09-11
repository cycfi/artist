/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]

   Asserts the claims of docs/modules/ROOT/pages/foundation/image.adoc. Each
   case names the page section it comes from. Behaviour that differs between
   backends and is under review is pinned in its own "current behaviour"
   cases at the bottom; those record what the library does today, not what
   it should do.
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

   // Page, Backend Differences: the channels of pixel (x, y) of an image
   // drawn through offscreen_image. Skia and Cairo give premultiplied
   // B, G, R, A bytes directly. On Quartz 2D pixels() of such an image is
   // not 32 bit pixels, so the image is saved and loaded back, which gives
   // straight R, G, B, A bytes.
   rgba8 pixel_at(image const& img, int x, int y)
   {
#if defined(ARTIST_QUARTZ_2D)
      static int n = 0;
      auto path = get_results_path() + "image_test_" + std::to_string(n++) + ".png";
      img.save_png(path);
      image loaded{fs::path{path}};
      auto p = reinterpret_cast<std::uint8_t const*>(loaded.pixels());
      p += 4 * (y * int(loaded.bitmap_size().x) + x);
      return {p[0], p[1], p[2], p[3]};
#else
      auto p = reinterpret_cast<std::uint8_t const*>(img.pixels());
      p += 4 * (y * int(img.bitmap_size().x) + x);
      return {p[2], p[1], p[0], p[3]};
#endif
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
#if defined(ARTIST_CAIRO) || defined(ARTIST_SKIA)
      REQUIRE(img.pixels() != nullptr);
      CHECK(img.bitmap_size() == extent{10, 20});
      for (int i = 0; i != 10 * 20; ++i)
         CHECK(img.pixels()[i] == 0);
#else
      CHECK(img.pixels() == nullptr);
      CHECK(img.bitmap_size() == extent{0, 0});
#endif
   }

   {
      image img{fs::path{get_images_path() + "logo.png"}};
      CHECK(img.size() == extent{512, 512});
   }

#if !defined(ARTIST_QUARTZ_2D)
   CHECK_THROWS_AS(image{missing_file}, std::runtime_error);
#endif

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

#if !defined(ARTIST_QUARTZ_2D)
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
#endif
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

#if !defined(ARTIST_QUARTZ_2D)
      // Skia and Cairo: premultiplied B, G, R, A bytes, and a bitmap the
      // same size as the image.
      CHECK(img.bitmap_size() == img.size());
      auto p = reinterpret_cast<std::uint8_t const*>(img.pixels());
      CHECK(p[0] == 255);
      CHECK(p[1] == 0);
      CHECK(p[2] == 0);
      CHECK(p[3] == 255);
#endif
   }

   // A loaded PNG: straight R, G, B, A on Quartz 2D, straight B, G, R, A on
   // Skia, premultiplied B, G, R, A on Cairo. logo.png is transparent at
   // (0, 0) and opaque (0x2e, 0x27, 0x6c) at (256, 256).
   {
      image logo{fs::path{get_images_path() + "logo.png"}};
      auto p = reinterpret_cast<std::uint8_t const*>(logo.pixels());
      REQUIRE(p != nullptr);
      auto w = int(logo.bitmap_size().x);
      auto q = p + 4 * (256 * w + 256);
      CHECK(p[3] == 0);
#if defined(ARTIST_QUARTZ_2D)
      CHECK(near({q[0], q[1], q[2], q[3]}, {0x2e, 0x27, 0x6c, 255}));
#else
      CHECK(near({q[2], q[1], q[0], q[3]}, {0x2e, 0x27, 0x6c, 255}));
#endif
#if defined(ARTIST_CAIRO)
      // Premultiplied: a transparent pixel has no colour left.
      CHECK((p[0] | p[1] | p[2]) == 0);
#endif
   }

   // A make_image image: the source format as given on Quartz 2D and Skia,
   // converted to premultiplied B, G, R, A on Cairo.
   {
      std::uint8_t buf[4] = {0x10, 0x20, 0x30, 0x40};
      auto img = make_image<pixel_format::gray8>(buf, {2, 2});
      auto p = reinterpret_cast<std::uint8_t const*>(img.pixels());
      REQUIRE(p != nullptr);
#if defined(ARTIST_CAIRO) || defined(ARTIST_SKIA)
      CHECK(p[0] == 0x10);
      CHECK(p[1] == 0x10);
      CHECK(p[2] == 0x10);
      CHECK(p[3] == 0xff);
      CHECK(p[4] == 0x20);
#else
      CHECK(p[0] == 0x10);
      CHECK(p[1] == 0x20);
      CHECK(p[2] == 0x30);
      CHECK(p[3] == 0x40);
#endif
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

#if !defined(ARTIST_SKIA)
   // Quartz 2D and Cairo draw over what the image already holds.
   fill(img, colors::blue, {0, 0, 5, 10});
   CHECK(near(pixel_at(img, 1, 5), blue));
   CHECK(near(pixel_at(img, 8, 5), red));
#endif
}

///////////////////////////////////////////////////////////////////////////////
// Current behaviour, under review. Each case below pins a backend difference
// the page documents under Backend Differences. They record what the library
// does today so that a change is visible; none asserts the behaviour is
// correct.
///////////////////////////////////////////////////////////////////////////////

TEST_CASE("Image current behaviour: load failure", "[image]")
{
#if defined(ARTIST_QUARTZ_2D)
   // Quartz 2D does not throw; the image is empty.
   CHECK_NOTHROW(image{missing_file});
   image img{missing_file};
   CHECK(img.size() == extent{0, 0});
#endif
}

TEST_CASE("Image current behaviour: save_png failure", "[image]")
{
   image img{4, 4};
   fill(img, colors::red, {0, 0, 4, 4});
#if defined(ARTIST_CAIRO) || defined(ARTIST_SKIA)
   CHECK_THROWS_AS(img.save_png(unwritable_file), std::runtime_error);
#else
   // Quartz 2D does not report the failure.
   CHECK_NOTHROW(img.save_png(unwritable_file));
#endif
}

TEST_CASE("Image current behaviour: make_image buffer lifetime", "[image]")
{
   // Overwrite the buffer after make_image and before the image is used.
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
#if defined(ARTIST_QUARTZ_2D)
   // Quartz 2D reads the buffer in place: the overwrite shows.
   CHECK(near(drawn(img), blue));
#else
   // Skia and Cairo copied it.
   CHECK(near(drawn(img), red));
#endif
}

TEST_CASE("Image current behaviour: rgba32 alpha", "[image]")
{
   // R = 0x80, A = 0x80. Read as straight alpha that is half red at half
   // opacity; read as premultiplied it is full red at half opacity.
   std::uint8_t buf[16] = {
      0x80, 0, 0, 0x80,  0x80, 0, 0, 0x80,  0x80, 0, 0, 0x80,  0x80, 0, 0, 0x80
   };
   auto img = make_image<pixel_format::rgba32>(
      reinterpret_cast<std::uint32_t const*>(buf), {2, 2});
   auto c = drawn(img);
#if defined(ARTIST_QUARTZ_2D)
   // Premultiplied. pixel_at gives straight values here.
   CHECK(near(c, {255, 0, 0, 0x80}));
#elif defined(ARTIST_CAIRO)
   // Straight. pixel_at gives premultiplied values here.
   CHECK(near(c, {0x40, 0, 0, 0x80}));
#else
   // Skia declares the bitmap kOpaque_SkAlphaType, and SkAlphaType.h says
   // drawing a pixel with alpha below 1.0 is then undefined. Nothing to pin.
   (void) c;
#endif
}

TEST_CASE("Image current behaviour: rgb16 and rgb32 on Quartz 2D", "[image]")
{
#if defined(ARTIST_QUARTZ_2D)
   {
      // rgb32 draws nothing.
      std::uint8_t buf[16] = {
         255, 0, 0, 0,  255, 0, 0, 0,  255, 0, 0, 0,  255, 0, 0, 0
      };
      auto img = make_image<pixel_format::rgb32>(
         reinterpret_cast<std::uint32_t const*>(buf), {2, 2});
      CHECK(drawn(img).a == 0);
   }
   {
      // rgb16 is not read as RGB 5-6-5. It reads a four byte row stride,
      // so the buffer is padded to twice its size to keep the read in
      // bounds.
      std::uint16_t buf[8] = {0xF800, 0xF800, 0xF800, 0xF800, 0, 0, 0, 0};
      auto img = make_image<pixel_format::rgb16>(buf, {2, 2});
      CHECK(!near(drawn(img), red));
   }
#endif
}

TEST_CASE("Image current behaviour: Quartz 2D offscreen bitmap", "[image]")
{
#if defined(ARTIST_QUARTZ_2D)
   // bitmap_size() is {0, 0} until pixels() has been called.
   image img{10, 10};
   fill(img, colors::red, {0, 0, 10, 10});
   CHECK(img.bitmap_size() == extent{0, 0});
   CHECK(img.pixels() != nullptr);
   CHECK(!(img.bitmap_size() == extent{0, 0}));
#endif
}

TEST_CASE("Image current behaviour: when drawing reaches the image", "[image]")
{
   // Save the image while the offscreen_image is still alive.
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
#if defined(ARTIST_CAIRO) || defined(ARTIST_SKIA)
   // Cairo and Skia draw straight into the image.
   CHECK(p[3] == 255);
#else
   // Quartz 2D commits the drawing when the offscreen_image is destroyed;
   // until then the image is still blank.
   CHECK(p[3] == 0);
#endif
   CHECK(near(pixel_at(img, 5, 5), red));
}