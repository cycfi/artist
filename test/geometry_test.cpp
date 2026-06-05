#include "test_support.hpp"

TEST_CASE("Scale and Coordinate Conversion")
{
   // Verify device_to_user / user_to_device are inverses of each other
   // for a plain offscreen image surface (identity initial CTM).
   image pm{window_size};
   {
      offscreen_image ctx{pm};
      canvas pm_cnv{ctx.context()};

      // With no transform: device coords == user coords.
      auto d = pm_cnv.user_to_device({100.0f, 200.0f});
      auto u = pm_cnv.device_to_user(d);
      CHECK(u.x == Approx(100.0f).epsilon(0.001));
      CHECK(u.y == Approx(200.0f).epsilon(0.001));

      // After applying a scale transform: verify inverse round-trip.
      pm_cnv.save();
      pm_cnv.scale(2.0f, 2.0f);

      auto d2 = pm_cnv.user_to_device({50.0f, 75.0f});
      auto u2 = pm_cnv.device_to_user(d2);
      CHECK(u2.x == Approx(50.0f).epsilon(0.001));
      CHECK(u2.y == Approx(75.0f).epsilon(0.001));

      // After scaling by 2, device coords should be 2x the user coords.
      CHECK(d2.x == Approx(100.0f).epsilon(0.001));
      CHECK(d2.y == Approx(150.0f).epsilon(0.001));

      pm_cnv.restore();
   }
}

TEST_CASE("Coordinate Conversion with Base Transform")
{
   // A host may hand the canvas a context whose CTM already encodes a base
   // transform (e.g. the GTK content-area / window-decoration offset).
   // device_to_user / user_to_device must be relative to that baseline so the
   // view origin maps to the host drawing origin -- not shifted by the base
   // offset.  (Regression: the Cairo backend used the absolute cairo_*_user
   // conversions, shifting all content by the decoration offset.)
   image pm{window_size};
   {
      offscreen_image ctx{pm};

      // Establish a non-identity base transform on the context, then build the
      // canvas that elements would draw through.  The constructor captures this
      // as its baseline.
      canvas base{ctx.context()};
      base.translate({26.0f, 60.0f});

      canvas pm_cnv{ctx.context()};

      // The view origin (0,0) maps to the host drawing origin, relative to the
      // captured baseline -- i.e. (0,0), not (26,60).
      auto origin = pm_cnv.user_to_device({0.0f, 0.0f});
      CHECK(origin.x == Approx(0.0f).margin(0.001));
      CHECK(origin.y == Approx(0.0f).margin(0.001));

      auto back = pm_cnv.device_to_user({0.0f, 0.0f});
      CHECK(back.x == Approx(0.0f).margin(0.001));
      CHECK(back.y == Approx(0.0f).margin(0.001));

      // An arbitrary point is unaffected by the base offset and round-trips.
      auto d = pm_cnv.user_to_device({40.0f, 70.0f});
      CHECK(d.x == Approx(40.0f).margin(0.001));
      CHECK(d.y == Approx(70.0f).margin(0.001));

      auto u = pm_cnv.device_to_user(d);
      CHECK(u.x == Approx(40.0f).margin(0.001));
      CHECK(u.y == Approx(70.0f).margin(0.001));
   }
}
TEST_CASE("Path Equality")
{
   // Identical paths are equal
   {
      path a, b;
      a.add_rect(10, 10, 100, 50);
      b.add_rect(10, 10, 100, 50);
      CHECK(a == b);
   }

   // Same path object is equal to itself
   {
      path a;
      a.add_circle(50, 50, 30);
      CHECK(a == a);
   }

   // Different geometry is not equal
   {
      path a, b;
      a.add_rect(10, 10, 100, 50);
      b.add_rect(10, 10, 100, 51);  // height differs
      CHECK(!(a == b));
   }

   // Different shape type is not equal
   {
      path a, b;
      a.add_rect(10, 10, 80, 80);
      b.add_circle(50, 50, 40);
      CHECK(!(a == b));
   }

   // Different fill rule makes paths not equal
   {
      path a, b;
      a.add_circle(50, 50, 30);
      a.add_circle(50, 50, 15);
      b.add_circle(50, 50, 30);
      b.add_circle(50, 50, 15);
      a.fill_rule(path::fill_winding);
      b.fill_rule(path::fill_odd_even);
      CHECK(!(a == b));
   }

   // Same fill rule with same geometry is equal
   {
      path a, b;
      a.add_circle(50, 50, 30);
      a.add_circle(50, 50, 15);
      a.fill_rule(path::fill_odd_even);
      b.add_circle(50, 50, 30);
      b.add_circle(50, 50, 15);
      b.fill_rule(path::fill_odd_even);
      CHECK(a == b);
   }

   // SVG-parsed paths with identical strings are equal
   {
      path a{"M 100 100 L 300 100 L 200 300 z"};
      path b{"M 100 100 L 300 100 L 200 300 z"};
      CHECK(a == b);
   }

   // SVG-parsed paths with different strings are not equal
   {
      path a{"M 100 100 L 300 100 L 200 300 z"};
      path b{"M 100 100 L 300 100 L 200 301 z"};
      CHECK(!(a == b));
   }
}

TEST_CASE("Image Pixel Round Trip")
{
   // Opaque white pixels must survive make_image<rgba32> → pixels() intact.
   // White is premultiplied-invariant so this holds across all backends.
   {
      constexpr int w = 4, h = 4;
      uint32_t src[w * h];
      // RGBA32: R=255, G=255, B=255, A=255
      std::fill(src, src + w * h, uint32_t(0xFFFFFFFF));

      auto img = make_image<pixel_format::rgba32>(src, {float(w), float(h)});
      auto* pix = img.pixels();
      REQUIRE(pix != nullptr);
      for (int i = 0; i < w * h; ++i)
         CHECK(pix[i] == 0xFFFFFFFF);
   }

   // Opaque black must also round-trip correctly (A=255, RGB=0).
   // On little-endian, rgba32 uint32 bytes are [R,G,B,A], so black+opaque = 0xFF000000.
   {
      constexpr int w = 2, h = 2;
      // RGBA32 little-endian: bytes[R=0,G=0,B=0,A=255] → uint32 = 0xFF000000
      // Cairo ARGB32 output: A=255,R=0,G=0,B=0 → uint32 = 0xFF000000
      uint32_t src[w * h];
      std::fill(src, src + w * h, uint32_t(0xFF000000));

      auto img = make_image<pixel_format::rgba32>(src, {float(w), float(h)});
      auto* pix = img.pixels();
      REQUIRE(pix != nullptr);
      for (int i = 0; i < w * h; ++i)
         CHECK(pix[i] == 0xFF000000);
   }
}

TEST_CASE("Color Maths")
{
  color a(0.2, 0.25, 0.5, 1.0);
  color b(0.5, 0.6, 0.1, 1.0);

  auto check = [&](color result, color check)
  {
    REQUIRE_THAT(result.red, Catch::WithinRel(check.red, 0.001f));
    REQUIRE_THAT(result.green, Catch::WithinRel(check.green, 0.001f));
    REQUIRE_THAT(result.blue, Catch::WithinRel(check.blue, 0.001f));
    REQUIRE_THAT(result.alpha, Catch::WithinRel(check.alpha, 0.001f));
  };

  check(a + b, color(0.7, 0.85, 0.6, 1.0));
  check(a - b, color(-0.3, -0.35, 0.4, 1.0));
  check(a * 2, color(0.4, 0.5, 1.0, 1.0));
  check(2 * a, color(0.4, 0.5, 1.0, 1.0));
}
