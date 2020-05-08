# Foundation

## Table of Contents
* [point](#point)
* [extent](#extent)
* [rect](#rect)
* [circle](#circle)
* [color](#color)
    * [Predefined Colors](#predefined-colors)
* [font_descr](#font_descr)
* [font](#font)
* [resources](#resources)

-------------------------------------------------------------------------------
The Foundation classes provide support for the Artist library, especially for
the `canvas` class.

-------------------------------------------------------------------------------
## point

The point indicates position in the 2D cartesian coordinate space,
represented by the `x` and `y` coordinates:

```c++
struct point
{
               point();
               point(float x, float y);
               point(point const&) = default;
   point&      operator=(point const &) = default;

   bool        operator==(point const& other) const;
   bool        operator!=(point const& other) const;

   point       move(float dx, float dy) const;
   point       move_to(float x, float y) const;
   point       reflect(point p) const;

   float       x;
   float       y;
};
```

### Expressions

```c++
// Default constructor [1].
point{}

// Constructor [2].
point{ x, y }

// Copy constructor. [3]
point{ p }

// Assignment [4]
p = p2

// Equality [5]
p == p2

// Non-equality [6]
p != p2

// Move [7]
p.move(dx, dy)

// Move To [8]
p.move_to(x, y)

// Reflect [9]
p.reflect(p2)

// Member access [10]
p.x
p.y
```

### Notation

| `x`, `y`     | Scalar coordinates. |
| `dx`, `dy`   | Relative scalar coordinates. |
| `p`, `p2`    | Instance of `point` |

### Semantics
1. Default construct a `point` with initial values `{ 0, 0 }`
2. Construct a `point` given initial values `x`, and `y`.
3. Copy construct a `point `, `p`.
4. Assign `p2`, to `p`.
5. Returns `true` if a `p2`, is equal to `p`.
6. Returns `true` if `p2`, is not equal to a `p`.
7. Move `p` `dx` and `dy` distance relative to `p`. Returns the moved
   instance of `point`.
8. Move `p` to absolute coordinates `x` and `y`. Returns the moved instance
   of `point`.
9. Reflect a `point`, `p`, at 180 degree rotation of point, `p2`. Returns the
   reflected instance of `point`.
10. Direct access to members `x` and `y`

-------------------------------------------------------------------------------
## extent

The `extent` struct is a specialization of `point` but deletes the members
`move`, `move_to`, and `reflect`. `extent` is intended for specifying
2-dimensional sizes.

```c++
struct extent : point
{
   using point::point;

               extent(point p);

   point       move(float dx, float dy) const = delete;
   point       move_to(float x, float y) const = delete;
   point       reflect(point p) const = delete;
};
```

-------------------------------------------------------------------------------
## rect

The `rect` struct represents a 2-dimensional rectangle specified by the
`left`, `top`, `right`, and `bottom` coordinates.

```c++
struct rect
{
               rect();
               rect(float left, float top, float right, float bottom);
               rect(point origin, float right, float bottom)

               rect(point top_left, point bottom_right);
               rect(float left, float top, extent size);
               rect(point origin, extent size);

               rect(rect const&) = default;
   rect&       operator=(rect const&) = default;

   bool        operator==(rect const& other) const;
   bool        operator!=(rect const& other) const;

   bool        is_empty() const;
   bool        includes(point p) const;
   bool        includes(rect const& other) const;

   float       width() const;
   void        width(float w);
   float       height() const;
   void        height(float h);
   extent      size() const;

   point       top_left() const;
   point       bottom_right() const;
   point       top_right() const;
   point       bottom_left() const;

   rect        move(float dx, float dy) const;
   rect        move_to(float x, float y) const;
   rect        inset(float x = 1.0, float y = 1.0) const;

   float       left;
   float       top;
   float       right;
   float       bottom;
};
```

### Free Functions

```c++
bool           is_valid(rect const& r);
bool           is_same_size(rect const& a, rect const& b);
bool           intersects(rect const& a, rect const& b);

point          center_point(rect const& r);
float          area(rect const& r);
rect           union_(rect const& a, rect const& b);
rect           intersection(rect const& a, rect const& b);

void           clear(rect& r);
rect           center(rect const& r, rect const& encl);
rect           center_h(rect const& r, rect const& encl);
rect           center_v(rect const& r, rect const& encl);
rect           align(rect const& r, rect const& encl, float x, float y);
rect           align_h(rect const& r, rect const& encl, float x);
rect           align_v(rect const& r, rect const& encl, float y);
rect           clip(rect const& r, rect const& encl);
```

### Expressions

```c++
// Default constructor [1].
rect{}

// Constructor [2].
rect{ left, top, right, bottom }

// Constructor [3].
rect{ origin, right, bottom }

// Constructor [4].
rect{ top_left, bottom_right }

// Constructor [5].
rect{ left, top, size }

// Constructor [6].
rect{ origin, size }

// Copy constructor [7].
rect{ r }

// Assignment [8]
r = r2

// Equality [9]
r == r2

// Non-equality [10]
r != r2

// Check for empty rectangle [11]
r.empty()

// Check for inclusion [12]
r.includes(p)
r.includes(r2)

// Get the width and height [13]
r.width()
r.height()

// Get the width and height [14]
r.height(w)
r.width(h)

// Get the size [15]
r.size()

// Get the rectangle corners [16]
r.top_left()
r.bottom_right()
r.top_right()
r.bottom_left()

// Move [17]
r.move(dx, dy)

// Move To [18]
r.move_to(x, y)

// Inset [19]
r.inset(x, y)
r.inset(xy)

// Check for validity [20]
is_valid(r)

// Check for size equality [21]
is_same_size(r, r2)

// Check for intersection [22]
intersects(r, r2)

// Compute the center point [23]
center_point(r)

// Compute the area [24]
area(r)

// Compute the union of two rectangles [25]
union_(r, r2)

// Compute the intersection of two rectangles [26]
intersection(r, r2)

// Clear a rectangle [27]
clear(r)

// Center a rectangle [28]
center(r, r2)
center_h(r, r2)
center_v(r, r2)

// Align a rectangle [29]
align(r, r2, x, y)
align_h(r, r2, x)
align_v(r, r2, y)

// Member access [10]
r.left
r.top
r.right
r.bottom
```

### Notation

| `left`, `top`, `right`, `bottom`     | Scalar coordinates.            |
| `x`, `y`, `w`, `h`                   | Scalar coordinates.            |
| `dx`, `dy`                           | Relative scalar coordinates.   |
| `origin`, `top_left`, `bottom_right` | Instance of `point`.           |
| `size`                               | Instance of `extent`.          |
| `r`, `r2`                            | Instance of `rect`.            |


### Semantics

1. Default construct a `rect` with initial values `{ 0, 0, 0, 0 }`
2. Construct a `rect` given initial values `left`, `top`, `right`, and `bottom`.
3. Construct a `rect` given `origin` (point), `right` and bottom`.
4. Construct a `rect` given `top_left`, `bottom_right` points.
5. Construct a `rect` given `left`, `top`, and `size` (extent).
6. Construct a `rect` given `origin`, and `size` (extent).
7. Copy construct a `rect `, `r`.
8. Assign `r2` to `r`.
9. Returns `true` if a `r2` is equal `r`.
10. Returns `true` if a `r2` is not equal to `r`.
11. Returns `true` if `r` is empty. Equivalent to `r.size() == extent{ 0, 0 }`.
12. Inclusion:
    1. Returns true if `p` is inside `r`.
    2. Returns true if `r2` is completely inside `r`.
13. Returns the width and height of the `r`. Returns a scalar value.
14. Sets the width and height of `r` to `width_` and `height_`, respectively.
15. Get the size of `r`. Returns an instance of `extent`.
16. Get the top-left, bottom-right, top-right, and bottom-left corners or
    rectangle, `r`. Returns an instance of `point`.
17. Move origin of `r` by `dx` and `dy` distance. Returns the moved instance
    of `rect`.
18. Move origin of `r` to absolute coordinates `x` and `y`. Returns the moved
    instance of `rect`.
19. Inset `r` by `x` and `y` (in each dimension), or by `xy` (in both x and y
    dimensions). The rect `r` is shrunk if `x`, `y` or `xy` are positive,
    otherwise, expanded if negative.
20. Return `true` if `r` is a valid rectangle (left <= right && top <=
    bottom).
21. Return `true` if `r` and `r2` have the same size. Equivalent to `r.size()
    == r2.size()`.
22. Return `true` if `r` intersects with `r2`.
23. Returns the center point of `r`. returns an instance of `point`.
24. Returns the area of `r`. Equivalent to `r.width() * r.height()`. Returns
    a scalar value.
25. Compute the union of two rectangles. Returns an instance of `rect`.
26. Compute the intersection of two rectangles. Returns an instance of
    `rect`.
27. Clear `r`. Equivalent to r = rect{};
28. Center `r` in `r2`. Returns an instance of `rect`.
    1. `center`: In both dimensions
    2. `center_h`: Horizontally
    3. `center_v`: Vertically
29. Align `r` in `r2`. Given scalar `x` and `y` values, 0.0 aligns `r` to the
    left or top, 1.0 aligns `r` to the right or bottom, 0.5 aligns `r` center
    or middle. Returns an instance of `rect`.
    1. `align`: In both dimensions by `x` and `y`, where `x` and `y` are
       fractional values from 0.0 to 1.0.
    2. `align_h`: Horizontally by `x`, where `x` is a fractional value from
       0.0 to 1.0.
    3. `align_v`: Vertically by `y`, where `y` is a fractional value from 0.0
       to 1.0.
20. Direct access to members `left`, `top`, `right`, and `bottom`

-------------------------------------------------------------------------------
## circle

The circle is represented by a center point and radius:

```c++
struct circle
{
               circle();
               circle(float cx, float cy, float radius);
               circle(point c, float radius);
               circle(rect const& r);
               circle(circle const&) = default;
   circle&     operator=(circle const&) = default;

   rect        bounds() const;
   bool        operator==(circle const& other) const;
   bool        operator!=(circle const& other) const;

   point       center() const;
   circle      inset(float x) const;
   circle      move(float dx, float dy) const;
   circle      move_to(float x, float y) const;

   float       cx;
   float       cy;
   float       radius;
};
```

### Expressions

```c++
// Default constructor [1].
circle{}

// Constructor [2].
circle{ x, y, radius }

// Copy constructor. [3]
circle{ c }

// Assignment [4]
c = c2

// Equality [5]
c == c2

// Non-equality [6]
c != c2

// Bounds [7]
c.bounds()

// Get the center [8]
c.center()

// Inset [9]
c.inset(x)

// Move [10]
c.move(dx, dy)

// Move To [11]
c.move_to(x, y)

// Member access [12]
c.cx
c.cy
c.radius
```

### Notation

| `x`, `y`, `radius`          | Scalar coordinates.   |
| `dx`, `dy`, `radius`        | Scalar coordinates.   |
| `c`, `c2`                   | Instance of `circle`. |

### Semantics

1. Default construct a `circle` with initial values `{ 0, 0, 0 }`
2. Construct a `circle` given initial values `x`, `y`, and `radius`.
3. Copy construct a `circle`, `c`.
4. Assign `c2` to `c`.
5. Returns `true` if a `c2` is equal `c`.
6. Returns `true` if a `c2` is not equal to `c`.
7. Get the bounds: smallest `rect` that encloses `c`. Returns an instance of
   `rect`.
8. Get the center of `c`. Returns an instance of `point`.
9. Inset `c` by `x`.
10. Move center of `c` by `dx` and `dy` distance. Returns the moved instance
    of `circle`.
11. Move center of `c` to absolute coordinates `x` and `y`. Returns the
    moved instance of `circle`.
12. Direct access to members `cx`, `cy`, and `radius`

-------------------------------------------------------------------------------
## color

Color is represented by `red`, `green`, `blue`, and `alpha`:

```c++
struct color
{
               color();
               color(float red, float green, float blue, float alpha = 1.0f);

   color       opacity(float alpha_) const;
   color       level(float amount) const;

   float       red   = 0.0f;
   float       green = 0.0f;
   float       blue  = 0.0f;
   float       alpha = 0.0f;
};
```

### Free Functions

```c++
bool     operator==(color const& a, color const& b);
bool     operator!=(color const& a, color const& b);
color    rgb(std::uint32_t rgb);
color    rgba(std::uint32_t rgba);
color    rgb(std::uint8_t r, std::uint8_t g, std::uint8_t b);
color    rgba(std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a);
color    hsl(float h, float sl, float l);
```

### Expressions

```c++
// Default constructor [1].
color{}

// Constructor [2].
color{ red, green, blue, alpha }

// Copy constructor. [3]
color{ c }

// Assignment [4]
c = c2

// Equality [5]
c == c2

// Non-equality [6]
c != c2

// Opacity
opacity(val) [7]

// Linear level
level(amount) [8]

// RGB and RGBA from uint32_t [9]
rgb(urgb);
rgba(urgb);

// RGB and RGBA from uint8_t reg, green, blue, and alpha components [10]
rgb(ured, ugreen, ublue);
rgba(ured, ugreen, ublue, ualpha);

// RGB from HSL [11]
hsl(h, sl, l);
```

### Notation

| `red`, `red`, `red`, `alpha`   | Scalar values.              |
| `val`, `amount`                | Scalar values.              |
| `c`, `c2`                      | Instance of `color`.        |
| `urgb`                         | Instance of `std::uint32_t  |
| `ured`, `ugreen`, `ublue`      | Instance of `std::uint8_t   |

### Semantics

1. Default construct a `color` with initial values `{ 0, 0, 0, 0 }`
2. Construct a `color` given initial values `red`, `green`, `blue`, and
   `alpha`. The floating point values range from 0.0 to 1.0.
3. Copy construct a `color`, `c`.
4. Assign `c2` to `c`.
5. Returns `true` if a `c2` is equal `c`.
6. Returns `true` if a `c2` is not equal to `c`.
7. Sets the opacity (alpha) to `val`. Returns an instance of `color`.
8. Multiplies all color components by `amount`. Returns an instance of `color`.
9. Create an RGB or RGBA color from a `uint32_t` or the form rrggbb or
   rrggbbaa respectively, typically with using hex literals (e.g.
   0xffffffff).
10. Create an RGB or RGBA color from `uint8_t` reg, green, blue, and alpha
    components, where each component range from `0` to `255`.
11. Create an RBB color from HSL.

### Predefined Colors

Namespace `cycfi::artist::colors` contains some predefined colors, including
256 levels of grays. You normally use the colors, by hoisting the namespace
into your own namespace:

```c++
namespace colors = cycfi::artist::colors;
```

The colors provided are from CSS: [Named Colors and Hex Equivalents](https://css-tricks.com/snippets/css/named-colors-and-hex-equivalents/)

#### The colors

```c++
alice_blue,
antique_white,
aquamarine,
azure,
beige,
bisque,
black,
blanched_almond,
blue,
blue_violet,
brown,
burly_wood,
cadet_blue,
chartreuse,
chocolate,
coral,
cornflower_blue,
corn_silk,
cyan,
dark_goldenrod,
dark_green,
dark_khaki,
dark_olive_green,
dark_orange,
dark_orchid,
dark_salmon,
dark_sea_green,
dark_slate_blue,
dark_slate_gray,
dark_turquoise,
dark_violet,
deep_pink,
deep_sky_blue,
dim_gray,
dodger_blue,
firebrick,
floral_white,
forest_green,
gains_boro,
ghost_white,
gold,
goldenrod,
green,
green_yellow,
honeydew,
hot_pink,
indian_red,
ivory,
khaki,
lavender,
lavender_blush,
lawn_green,
lemon_chiffon,
light_blue,
light_coral,
light_cyan,
light_goldenrod,
light_goldenrod_yellow,
light_gray,
light_pink,
light_salmon,
light_sea_green,
light_sky_blue,
light_slate_blue,
light_slate_gray,
light_steel_blue,
light_yellow,
lime_green,
linen,
magenta,
maroon,
medium_aquamarine,
medium_blue,
medium_forest_green,
medium_goldenrod,
medium_orchid,
medium_purple,
medium_sea_green,
medium_slate_blue,
medium_spring_green,
medium_turquoise,
medium_violet_red,
midnight_blue,
mint_cream,
misty_rose,
moccasin,
navajo_white,
navy,
navy_blue,
old_lace,
olive_drab,
orange,
orange_red,
orchid,
pale_goldenrod,
pale_green,
pale_turquoise,
pale_violet_red,
papaya_whip,
peach_puff,
peru,
pink,
plum,
powder_blue,
purple,
red,
rosy_brown,
royal_blue,
saddle_brown,
salmon,
sandy_brown,
sea_green,
sea_shell,
sienna,
sky_blue,
slate_blue,
slate_gray,
snow,
spring_green,
steel_blue,
tan,
thistle,
tomato,
turquoise,
violet,
violet_red,
wheat,
white,
white_smoke,
yellow,
yellow_green

// greys
gray[0] ... gray[255]
auto grey = gray; // Synonym
```

-------------------------------------------------------------------------------
## font_descr

`font_desr` is a lightweight class that describes a font, including its
weight (e.g. bold), slant (e.g. italic), and stretch (e.g. condensed).

```c++
namespace font_constants
{
   enum weight_enum
   {
      thin              = 10,
      extra_light       = 20,
      light             = 30,
      weight_normal     = 40,
      medium            = 50,
      semi_bold         = 60,
      bold              = 70,
      extra_bold        = 80,
      black             = 90,
      extra_black       = 95,
   };

   enum slant_enum
   {
      slant_normal      = 0,
      italic            = 90,
      oblique           = 100
   };

   enum stretch_enum
   {
      ultra_condensed   = 25,
      extra_condensed   = 31,
      condensed         = 38,
      semi_condensed    = 44,
      stretch_normal    = 50,
      semi_expanded     = 57,
      expanded          = 63,
      extra_expanded    = 75,
      ultra_expanded    = 100
   };
}

struct font_descr
{
   font_descr           normal() const;
   font_descr           size(float size_) const;

   font_descr           weight(font_constants::weight_enum w) const;
   font_descr           thin() const;
   font_descr           extra_light() const;
   font_descr           light() const;
   font_descr           weight_normal() const;
   font_descr           medium() const;
   font_descr           semi_bold() const;
   font_descr           bold() const;
   font_descr           extra_bold() const;
   font_descr           black() const;
   font_descr           extra_black() const;

   font_descr           style(font_constants::slant_enum s) const;
   font_descr           slant_normal() const;
   font_descr           italic() const;
   font_descr           oblique() const;

   font_descr           stretch(font_constants::stretch_enum s) const;
   font_descr           ultra_condensed() const;
   font_descr           extra_condensed() const;
   font_descr           condensed() const;
   font_descr           semi_condensed() const;
   font_descr           stretch_normal() const;
   font_descr           semi_expanded() const;
   font_descr           expanded() const;
   font_descr           extra_expanded() const;
   font_descr           ultra_expanded() const;

   using slant_enum = font_constants::slant_enum;

   std::string_view     _families;
   float                _size = 12;
   uint8_t              _weight = font_constants::weight_normal;
   slant_enum           _slant = font_constants::slant_normal;
   uint8_t              _stretch = font_constants::stretch_normal;
};
```

### Expressions

```c++
// Constructors [1].
font_descr{}
font_descr{ families }
font_descr{ families, sz }
font_descr{ families, sz, wt }
font_descr{ families, sz, wt, sl }
font_descr{ families, sz, wt, sl, str }

// Normal [2]
f.normal()

// Sized [3]
f.size(size)

// Weight [4]
f.weight(wt)
f.thin()
f.extra_light()
f.light()
f.weight_normal()
f.medium()
f.semi_bold()
f.bold()
f.extra_bold()
f.black()
f.extra_black()

// Slant [5]
f.style(sl)
f.slant_normal()
f.italic()
f.oblique()

// Stretch [6]
f.stretch(str)
f.ultra_condensed()
f.extra_condensed()
f.condensed()
f.semi_condensed()
f.stretch_normal()
f.semi_expanded()
f.expanded()
f.extra_expanded()
f.ultra_expanded()

// Member access [7]
f._families
f._size
f._weight
f._slant
f._stretch
```

### Notation

| `families`   | Comma separated list of font families to search for.               |
| `sz`         | A scalar value.                                                    |
| `wt`         | A `weight_enum` (see `font_constants` above) or a scalar value.    |
| `sl`         | A `slant_enum` (see `font_constants` above) or a scalar value.     |
| `str`        | A `stretch_enum` (see `font_constants` above) or a scalar value.   |
| `f`          | An instance of `font_descr`.                                       |

> :point_right: `families` is a comma separated list of font families to
> search for. Given a `font_descr`, the `font` class will search the system
> for a match, iterating over the list of font families, and will load first
> available font that matches the font descriptor.

### Semantics
1. Constructors for the `font_descr` struct given initial values.
2. Returns a copy of the `font_desr`, `f`, with default values. Returns
   instance of `font_descr`.
3. Returns a copy of the `font_desr`, `f`, with the given font size. Returns
   instance of `font_descr`.
4. Returns a copy of the `font_desr`, `f`, with the given font weight.
   Returns instance of `font_descr`.
5. Returns a copy of the `font_desr`, `f`, with the given font slant. Returns
   instance of `font_descr`.
6. Returns a copy of the `font_desr`, `f`, with the given font stretch.
   Returns instance of `font_descr`.
7. Direct access to members.

> :point_right: The syntax allows for concatenation. For example:
> ```c++
> font_descr{ "Open Sans", 14 }.bold().italic()
> ```

-------------------------------------------------------------------------------
## font

The font loads a system or application font given a font descriptor.

```c++
class font
{
public:
                        font();
                        font(font_descr descr);
                        font(font const& rhs);
                        font(font&& rhs) noexcept;
                        ~font();

   font&                operator=(font const& rhs);
   font&                operator=(font&& rhs) noexcept;
   explicit             operator bool() const;
   font_impl_ptr        impl() const;

   struct metrics_info
   {
      float             ascent;
      float             descent;
      float             leading;
   };

   metrics_info         metrics() const;
   float                line_height() const;
   float                measure_text(std::string_view str) const;
};
```

### Expressions

```c++
// Default constructor [1].
font{}

// Constructor [2].
font{ fd }

// Copy constructor. [3]
font{ f }

// Assignment [4]
f = f2

// Validity Check [5]
bool(f)

// Access to font handle [6]
f.impl()

// Font metrics [7]
f.metrics()
f.line_height()

// Measuring text [8]
f.measure_text(str)
```

### Notation

| `fd`         | Instance of `font_descr`       |
| `f`          | Instance of `font`             |
| `str`        | Instance of `std::string_view` |

### Semantics
1. Default construct a `font`. The default constructed font is invalid.
2. Construct a `point` given a font descriptor `font_descr`, `fd`.
3. Copy construct a `font`, `f`.
4. Assign `f2`, to `f`.
5. Check if a `f` is valid. Returns `true` if `f` is valid.
6. Get the backend-specific font handle.
7. Get the font metrics (`ascent`, `descent`, `leading` and the
   `line_height`).
    - `f.metrics()` returns an instance of `font::metrics_info`.
    - `f.line_height()` returns a scalar value.
8. Measure the length the given string, `str`, will occupy when drawn using
   the font `f`. This does not take into account the `canvas`' transformation
   matrix. Returns a scalar value.

-------------------------------------------------------------------------------
## resources

Resources (e.g. images) that are identified by file names can be absolute or
relative paths. For relative paths, a vector of paths is searched, in the
order they appear in the vector. Platform code provides the initial paths as
appropriate. Applications may add additional paths as needed, using the
add_search_path function. search_first=true puts the path at the top of the
vector and will be searched first.

```c++
void        add_search_path(fs::path const& path, bool search_first = false);
fs::path    find_file(fs::path const& file);
fs::path    app_data_path();
```

### Expressions

```c++

// Add a search path [1]
add_search_path(path)
add_search_path(path, true)

// Search for a file [2]
find_file(path)
```

### Notation

| `path`       | Instance of `std::filesystem::path`   |

### Semantics
1. Add a directory search path. This path, and all paths added via
   `add_search_path`,  will be used when searching for a resource file. The
   second variant, with the second argument, specifies if the directory
   search path will be pushed into the end (default `false`) or pushed to the
   front (second argument is `true`) of the list. If `true`, the directory
   search path will be searched first.
2. Find a resource file by searching the list of directory search paths.
   Returns an empty path if file is not found. returns an instance of
   `std::filesystem::path`.

-------------------------------------------------------------------------------

*Copyright (c) 2014-2020 Joel de Guzman. All rights reserved.*
*Distributed under the [MIT License](https://opensource.org/licenses/MIT)*
