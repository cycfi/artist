# Canvas

## Table of Contents
* hhh

-------------------------------------------------------------------------------
The canvas API is a derivative of and in line with the spirit the HTML5
canvas API, but using a slightly different syntax and naming convention
following standard c++ style (e.g. using lower-case and underscores as word
separators instead of camelCase).

## Foundation

### Point

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

#### Expressions

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

#### Notation

| `x`, `y`     | Scalar coordinates. |
| `dx`, `dy`   | Relative scalar coordinates. |
| `p`, `p2`    | Instance of `point` |

#### Semantics
1. Default construct a `point` with initial values `{ 0, 0 }`
2. Construct a `point` given initial values `{ x, y }`.
3. Copy construct a `point ` given a `point`, `p`.
4. Assign `point`, `p2`, to `point`, `p`.
5. Returns true if a point `point`, `p2`, is equal to a `point`, `p`.
6. Returns true if a point `point`, `p2`, is not equal to a `point`, `p`.
7. Move a `point`, `p`, `dx` and `dy` distance relative to `point`, `p`.
   Returns the moved `point`.
8. Move a `point`, `p`, to absolute coordinates `x` and `y`. Returns the new
   `point`.
9. Reflect a `point`, `p`, at 180 degree rotation of point, `p2`. Returns the
   reflected `point`.
10. Direct access to members `x` and `y`

### Extent

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

The `extent` struct is a specialization of `point` but deletes the members
`move`, `move_to`, and `reflect`. `extent` is intended for specifying
2-dimensional sizes.

### Rect

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
   bool        includes(rect other) const;

   float       width() const;
   void        width(float width_);
   float       height() const;
   void        height(float height_);
   extent      size() const;

   point       top_left() const;
   point       bottom_right() const;
   point       top_right() const;
   point       bottom_left() const;

   rect        move(float dx, float dy) const;
   rect        move_to(float x, float y) const;
   rect        inset(float x_inset = 1.0, float y_inset = 1.0) const;

   float       left;
   float       top;
   float       right;
   float       bottom;
};
```

#### Free Functions

```c++
bool           is_valid(rect r);
bool           is_same_size(rect a, rect b);
bool           intersects(rect a, rect b);

point          center_point(rect r);
float          area(rect r);
rect           union_(rect a, rect b);
rect           intersection(rect a, rect b);

void           clear(rect& r);
rect           center(rect r, rect encl);
rect           center_h(rect r, rect encl);
rect           center_v(rect r, rect encl);
rect           align(rect r, rect encl, float x_align, float y_align);
rect           align_h(rect r, rect encl, float x_align);
rect           align_v(rect r, rect encl, float y_align);
rect           clip(rect r, rect encl);
```

#### Expressions

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
rect{ left, top, extent }

// Constructor [6].
rect{ origin, extent }

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
r.height(width_)
r.width(height_)

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

// Compute the union or two rectangles [25]
union_(r, r2)

// Compute the intersection or two rectangles [26]
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

// Clip a rectangle [30]
clip(r, r2)
```

-------------------------------------------------------------------------------

*Copyright (c) 2014-2020 Joel de Guzman. All rights reserved.*
*Distributed under the [MIT License](https://opensource.org/licenses/MIT)*
