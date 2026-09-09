/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]

   Unit test for the foundation value types: affine_transform, color and
   circle. Every case here asserts a claim made by the corresponding
   reference page under docs/modules/ROOT/pages/foundation/, so the pages
   and the library cannot drift apart silently. Non-graphical: builds and
   runs without a window or graphics backend.
=============================================================================*/
#include <artist/affine_transform.hpp>
#include <artist/circle.hpp>
#include <artist/color.hpp>
#include <cmath>
#include <iostream>
#include <iterator>
#include <type_traits>

using namespace cycfi::artist;

static int failures = 0;
// Variadic so a braced initializer list inside the condition, which the
// value types here are full of, is not mistaken for extra macro arguments.
#define CHECK(...) do { if (!(__VA_ARGS__)) { \
   std::cerr << "FAIL " << __LINE__ << ": " #__VA_ARGS__ "\n"; \
   ++failures; } } while (0)

static bool near_(double a, double b, double eps = 1e-5)
{
   return std::fabs(a - b) <= eps;
}

static bool near_(point a, point b, double eps = 1e-5)
{
   return near_(a.x, b.x, eps) && near_(a.y, b.y, eps);
}

static bool near_(color a, color b, double eps = 1e-5)
{
   return near_(a.red, b.red, eps) && near_(a.green, b.green, eps)
      && near_(a.blue, b.blue, eps) && near_(a.alpha, b.alpha, eps);
}

///////////////////////////////////////////////////////////////////////////
// affine_transform
///////////////////////////////////////////////////////////////////////////

// The page calls the type "a plain aggregate of six double members" and
// says everything except rotate and skew is constexpr.
static_assert(std::is_aggregate_v<affine_transform>);
static_assert(affine_transform{}.is_identity());
static_assert(affine_identity.a == 1.0 && affine_identity.d == 1.0);
static_assert(make_translation(3.0, 4.0).tx == 3.0);
static_assert(make_scale(2.0).apply(point{3.0f, 4.0f}).x == 6.0f);

static void test_affine_construction()
{
   affine_transform t;
   CHECK(t.a == 1.0 && t.b == 0.0 && t.c == 0.0);
   CHECK(t.d == 1.0 && t.tx == 0.0 && t.ty == 0.0);
   CHECK(t.is_identity());
   CHECK(t == affine_identity);

   // The page's NOTE: there are no user declared constructors, so members
   // may be given in part; affine_transform{2.0} is a scale of 2 in x only.
   affine_transform sx{2.0};
   CHECK(sx.a == 2.0 && sx.d == 1.0);
   CHECK(near_(sx.apply(point{3.0f, 5.0f}), point{6.0f, 5.0f}));

   // Comparison is exact.
   affine_transform almost{1.0 + 1e-12, 0, 0, 1, 0, 0};
   CHECK(almost != affine_identity);
   CHECK(!almost.is_identity());
}

static void test_affine_factories()
{
   CHECK(make_translation(3.0, 4.0) == affine_transform{1, 0, 0, 1, 3, 4});
   CHECK(make_scale(2.0, 3.0) == affine_transform{2, 0, 0, 3, 0, 0});
   CHECK(make_scale(2.0) == affine_transform{2, 0, 0, 2, 0, 0});

   auto r = make_rotation(M_PI / 2);
   CHECK(near_(r.a, 0.0) && near_(r.b, 1.0));
   CHECK(near_(r.c, -1.0) && near_(r.d, 0.0));

   auto k = make_skew(0.25, 0.5);
   CHECK(near_(k.b, std::tan(0.25)) && near_(k.c, std::tan(0.5)));
   CHECK(k.a == 1.0 && k.d == 1.0);
}

static void test_affine_composition()
{
   auto t = make_translation(10.0, 20.0);
   auto s = make_scale(2.0, 4.0);
   point p{3.0f, 5.0f};

   // The page: t * t2 applies t2 first and then t.
   CHECK(near_((t * s).apply(p), t.apply(s.apply(p))));

   // Composition is not commutative.
   CHECK((t * s) != (s * t));

   // The page: the composing members are t * make_translation(...) and so
   // on, so the new operation applies within the space t establishes.
   // Scaling by 2 and then translating by 10 moves by 20 device units.
   auto composed = affine_identity.scale(2.0).translate(10.0, 0.0);
   CHECK(near_(composed.apply(point{0.0f, 0.0f}).x, 20.0));
   CHECK(composed == make_scale(2.0) * make_translation(10.0, 0.0));

   CHECK(affine_identity.translate(3.0, 4.0) == make_translation(3.0, 4.0));
   CHECK(affine_identity.scale(2.0, 3.0) == make_scale(2.0, 3.0));
}

static void test_affine_apply()
{
   auto t = make_scale(2.0, 3.0) * make_translation(1.0, 1.0);

   CHECK(near_(t.apply(point{4.0f, 5.0f}), t.apply(4.0f, 5.0f)));

   point pts[3] = {{0, 0}, {1, 1}, {2, 2}};
   point expect[3] = {t.apply(pts[0]), t.apply(pts[1]), t.apply(pts[2])};

   point a[3] = {{0, 0}, {1, 1}, {2, 2}};
   t.apply<3>(a);
   for (int i = 0; i != 3; ++i)
      CHECK(near_(a[i], expect[i]));

   point b[3] = {{0, 0}, {1, 1}, {2, 2}};
   t.apply(b, 3);
   for (int i = 0; i != 3; ++i)
      CHECK(near_(b[i], expect[i]));
}

static void test_affine_inversion()
{
   auto t = make_translation(10.0, 20.0) * make_scale(2.0, 4.0);
   auto inv = t.invert();
   point p{3.0f, 5.0f};
   CHECK(near_(inv.apply(t.apply(p)), p));

   // The page's IMPORTANT: a zero determinant is not invertible and t
   // itself is returned unchanged, with no error reported.
   affine_transform singular{0, 0, 0, 0, 5, 7};
   CHECK(near_(singular.a * singular.d - singular.c * singular.b, 0.0));
   CHECK(singular.invert() == singular);
}

///////////////////////////////////////////////////////////////////////////
// color
///////////////////////////////////////////////////////////////////////////

// The page: color declares its own constructors, so it is not an
// aggregate; the whole type is constexpr, members included.
static_assert(!std::is_aggregate_v<color>);
static_assert(color{}.alpha == 0.0f);
static_assert(color{1.0f, 0.0f, 0.0f}.alpha == 1.0f);
static_assert(color{1.0f, 1.0f, 1.0f}.opacity(0.5f).alpha == 0.5f);
static_assert(rgb(0xff0000).red == 1.0f);

static void test_color_construction()
{
   // Default is transparent black, not opaque black.
   color d;
   CHECK(d.red == 0.0f && d.green == 0.0f && d.blue == 0.0f);
   CHECK(d.alpha == 0.0f);
   CHECK(d != colors::black);
   CHECK(colors::black.alpha == 1.0f);

   // Three components pick up the default argument and are opaque.
   CHECK(near_(color{0.25f, 0.5f, 0.75f}, color{0.25f, 0.5f, 0.75f, 1.0f}));

   // Comparison is exact.
   CHECK(color{0.5f, 0, 0, 1} != color{0.5f + 1e-7f, 0, 0, 1});
}

static void test_color_derivation()
{
   constexpr auto c = color{0.2f, 0.4f, 0.6f, 0.8f};

   // opacity replaces the alpha and leaves the components untouched.
   auto o = c.opacity(0.25f);
   CHECK(near_(o, color{0.2f, 0.4f, 0.6f, 0.25f}));

   // level scales the components and leaves the alpha untouched.
   auto l = c.level(0.5f);
   CHECK(near_(l, color{0.1f, 0.2f, 0.3f, 0.8f}));

   // Neither modifies c; both are const.
   CHECK(near_(c, color{0.2f, 0.4f, 0.6f, 0.8f}));

   // level does not clamp.
   CHECK(near_(c.level(2.0f).blue, 1.2f));

   // c * k is equivalent to c.level(k).
   CHECK(near_(c * 0.5f, l));
   CHECK(near_(0.5f * c, l));
}

static void test_color_free_functions()
{
   CHECK(near_(rgb(0x336699), color{0.2f, 0.4f, 0.6f, 1.0f}, 1e-3));
   CHECK(near_(rgb(0x33, 0x66, 0x99), rgb(0x336699)));

   // rgba packs as 0xRRGGBBAA.
   auto a = rgba(0x336699ccu);
   CHECK(near_(a, color{0.2f, 0.4f, 0.6f, 0.8f}, 1e-2));
   CHECK(near_(rgba(0x33, 0x66, 0x99, 0xcc), a));

   // hsl: hue in degrees, saturation and lightness 0..1, alpha 1.
   CHECK(near_(hsl(0.0f, 1.0f, 0.5f), color{1, 0, 0, 1}, 1e-3));
   CHECK(near_(hsl(120.0f, 1.0f, 0.5f), color{0, 1, 0, 1}, 1e-3));
   CHECK(near_(hsl(240.0f, 1.0f, 0.5f), color{0, 0, 1, 1}, 1e-3));
   CHECK(hsl(90.0f, 0.5f, 0.25f).alpha == 1.0f);

   // The page's NOTE: h is clamped to 359.99 before converting, so
   // hsl(360, s, l) is close to but not exactly hsl(0, s, l).
   CHECK(hsl(360.0f, 1.0f, 0.5f) != hsl(0.0f, 1.0f, 0.5f));
   CHECK(near_(hsl(360.0f, 1.0f, 0.5f), hsl(0.0f, 1.0f, 0.5f), 1e-3));
}

static void test_predefined_colors()
{
   // The page: 101 entries, one per percent, black to white. Not 256.
   CHECK(std::size(colors::gray) == 101);
   CHECK(near_(colors::gray[0], color{0, 0, 0, 1}));
   CHECK(near_(colors::gray[100], color{1, 1, 1, 1}));
   CHECK(near_(colors::gray[50].red, 127.0f / 255.0f, 1e-3));

   // Monotonic and neutral all the way up.
   for (std::size_t i = 0; i != std::size(colors::gray); ++i)
   {
      auto g = colors::gray[i];
      CHECK(g.red == g.green && g.green == g.blue);
      if (i)
         CHECK(colors::gray[i - 1].red < g.red);
   }

   // grey is a synonym, but the array decays: it is a pointer.
   static_assert(std::is_same_v<decltype(colors::grey), color const* const>);
   CHECK(near_(colors::grey[42], colors::gray[42]));

   // Three name pairs hold the same value.
   CHECK(colors::tan == colors::burly_wood);
   CHECK(colors::salmon == colors::dark_salmon);
   CHECK(colors::navy == colors::navy_blue);

   // The values are not all the CSS ones. green is what CSS calls lime,
   // and purple and orange are the X11 values.
   CHECK(colors::green == rgb(0, 255, 0));
   CHECK(colors::purple == rgb(160, 32, 240));
   CHECK(colors::orange == rgb(255, 135, 0));
}

///////////////////////////////////////////////////////////////////////////
// circle
///////////////////////////////////////////////////////////////////////////

static_assert(!std::is_aggregate_v<circle>);
static_assert(circle{}.radius == 0.0f);
static_assert(circle{1.0f, 2.0f, 3.0f}.cx == 1.0f);

static void test_circle_construction()
{
   circle d;
   CHECK(d.cx == 0.0f && d.cy == 0.0f && d.radius == 0.0f);

   circle c{10.0f, 20.0f, 5.0f};
   CHECK(c.cx == 10.0f && c.cy == 20.0f && c.radius == 5.0f);
   CHECK(c == circle{point{10.0f, 20.0f}, 5.0f});

   CHECK(c != circle{10.0f, 20.0f, 5.0f + 1e-6f});
}

static void test_circle_accessors()
{
   circle c{10.0f, 20.0f, 5.0f};
   CHECK(near_(c.center(), point{10.0f, 20.0f}));

   auto b = c.bounds();
   CHECK(near_(b.left, 5.0) && near_(b.top, 15.0));
   CHECK(near_(b.right, 15.0) && near_(b.bottom, 25.0));
   CHECK(near_(b.width(), 10.0) && near_(b.height(), 10.0));
}

static void test_circle_derivation()
{
   circle c{10.0f, 20.0f, 5.0f};

   CHECK(c.inset(2.0f) == circle{10.0f, 20.0f, 3.0f});
   CHECK(c.inset(-2.0f) == circle{10.0f, 20.0f, 7.0f});
   CHECK(c.move(1.0f, 2.0f) == circle{11.0f, 22.0f, 5.0f});

   // move_to places the centre, where rect::move_to places the top left.
   CHECK(c.move_to(1.0f, 2.0f) == circle{1.0f, 2.0f, 5.0f});
   CHECK(near_(rect{0, 0, 10, 10}.move_to(1.0f, 2.0f).left, 1.0));

   // None of them modify c.
   CHECK(c == circle{10.0f, 20.0f, 5.0f});

   // The page's IMPORTANT: inset does not clamp. Insetting past the radius
   // gives a negative radius whose bounds() is not a valid rect.
   auto over = c.inset(8.0f);
   CHECK(near_(over.radius, -3.0));
   CHECK(!is_valid(over.bounds()));
}

///////////////////////////////////////////////////////////////////////////
// Behaviour under review. These pin down what the library does today so a
// change is visible; both are flagged in the reference pages as suspect
// and neither is asserted to be correct. See docs foundation/color.adoc
// (Arithmetic) and foundation/circle.adoc (Constructors and Assignment).
///////////////////////////////////////////////////////////////////////////

static void test_color_arithmetic_current_behaviour()
{
   constexpr auto a = color{0.2f, 0.4f, 0.6f, 0.5f};
   constexpr auto b = color{0.1f, 0.1f, 0.1f, 0.25f};

   // The components add and subtract componentwise, unclamped.
   CHECK(near_((a + b).red, 0.3f));
   CHECK(near_((a - b).red, 0.1f));
   CHECK(near_((color{0.1f, 0, 0, 1} - color{0.5f, 0, 0, 1}).red, -0.4f));

   // The alpha of a + b is the source over rule.
   auto over = 0.5f + 0.25f * (1.0f - 0.5f);
   CHECK(near_((a + b).alpha, over));

   // REVIEW: the alpha of a - b uses the same source over rule rather than
   // a difference, so subtraction raises the alpha. The existing Color
   // Maths case in geometry_test.cpp cannot see this: it builds both
   // operands with alpha 1.0, the one value where the two agree.
   CHECK(near_((a - b).alpha, over));
   CHECK(near_((a - b).alpha, (a + b).alpha));

   // c * k leaves the alpha alone.
   CHECK(near_((a * 2.0f).alpha, 0.5f));
}

static void test_circle_from_rect_current_behaviour()
{
   // REVIEW: the constructor takes the smaller side as the radius, not as
   // the diameter, so the circle is twice the size that fits the rect.
   rect r{0, 0, 100, 60};
   circle c{r};

   CHECK(near_(c.center(), center_point(r)));
   CHECK(near_(c.radius, 60.0));                  // std::min(w, h)
   CHECK(near_(c.bounds().width(), 120.0));       // 2x the rect height
   CHECK(near_(c.bounds().height(), 120.0));
   CHECK(!r.includes(c.bounds()));

   // The inscribed circle a caller most likely wants.
   circle fits{center_point(r), std::min(r.width(), r.height()) / 2};
   CHECK(near_(fits.radius, 30.0));
   CHECK(r.includes(fits.bounds()));
}

int main()
{
   test_affine_construction();
   test_affine_factories();
   test_affine_composition();
   test_affine_apply();
   test_affine_inversion();

   test_color_construction();
   test_color_derivation();
   test_color_free_functions();
   test_predefined_colors();

   test_circle_construction();
   test_circle_accessors();
   test_circle_derivation();

   test_color_arithmetic_current_behaviour();
   test_circle_from_rect_current_behaviour();

   if (failures)
      std::cerr << failures << " check(s) failed\n";
   else
      std::cout << "foundation_test: all checks passed\n";
   return failures ? 1 : 0;
}
