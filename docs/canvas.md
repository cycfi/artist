# Canvas

## Table of Contents
* [path](#path)
* [image](#image)
* [text_layout](#text_layout)
* [canvas](#canvas)

-------------------------------------------------------------------------------
The canvas API is a derivative of and in line with the spirit the HTML5
canvas API, but using a slightly different syntax and naming convention
following standard c++ style (e.g. using lower-case and underscores as word
separators instead of camelCase).

The canvas API is built on top of [Foundation classes](foundation) such as
points and rectangles and some more drawable classes included in the page,
such as paths and images. The `path` encapsulates compound (multiple contour)
geometric paths consisting of straight line segments, quadratic curves, and
cubic curves.

-------------------------------------------------------------------------------
## path

The `path` is the basic building block for a lot of drawing tasks that
involve the `canvas`. The path is a mathematical description of shapes, that
include lines, rectangles, cubic and quadratic curves, circles, arcs, etc.
Each shape in the path is constructed using a set of primitives that
represent instructions on how the path is formed.

For efficiency, it is advisable to construct a `path` once and draw it
multiple times. Graphics backends, especially those that involve the GPU, are
optimized for immutable paths that are created outside the UI draw function.

```c++
class path
{
public:
                     path();
                     ~path();

                     path(rect const& r);
                     path(rect const& r, float radius);
                     path(circle const& c);
                     path(std::string_view svg_def);
                     path(path const& rhs);
                     path(path&& rhs);

   path&             operator=(path const& rhs);
   path&             operator=(path&& rhs);

   bool              operator==(path const& rhs) const;
   bool              operator!=(path const& rhs) const;
   bool              is_empty() const;
   bool              includes(point p) const;
   bool              includes(float x, float y) const;
   rect              bounds() const;

   void              close();

   void              add_rect(rect const& r);
   void              add_round_rect(rect const& r, float radius);
   void              add_circle(circle const& c);

   void              add_rect(float x, float y, float width, float height);
   void              add_round_rect(
                        float x, float y,
                        float width, float height,
                        float radius
                     );
   void              add_circle(float cx, float cy, float radius);

   void              move_to(point p);
   void              line_to(point p);
   void              arc_to(point p1, point p2, float radius);
   void              arc(
                        point p, float radius,
                        float start_angle, float end_angle,
                        bool ccw = false
                     );

   void              quadratic_curve_to(point cp, point end);
   void              bezier_curve_to(point cp1, point cp2, point end);

   void              move_to(float x, float y);
   void              line_to(float x, float y);
   void              arc_to(
                        float x1, float y1,
                        float x2, float y2,
                        float radius
                     );
   void              arc(
                        float x, float y, float radius,
                        float start_angle, float end_angle,
                        bool ccw = false
                     );

   void              quadratic_curve_to(float cpx, float cpy, float x, float y);
   void              bezier_curve_to(
                        float cp1x, float cp1y,
                        float cp2x, float cp2y,
                        float x, float y
                     );

   enum fill_rule_enum
   {
      fill_winding,
      fill_odd_even
   };

   void              fill_rule(fill_rule_enum rule);

   path_impl*        impl();
   path_impl const*  impl() const;
};
```

### Expressions

```c++
// Default constructor [1].
path{}

// Constructors [2].
path{ r }
path{ r, radius }
path{ c }
path{ svg_def }

// Copy constructor. [3]
path{ p }

// Assignment [4]
p = p2

// Equality [5]
p == p2

// Non-equality [6]
p != p2

// Check for empty path [7]
p.empty()

// Check for inclusion [8]
p.includes(pt)
p.includes(x, y)

// Get the path bounds [9]
p.bounds()

// Close path [10]
p.close()

// Add rectangle [11]
p.add_rect(r)
p.add_rect(x, y, width height)

// Add rounded rectangle [12]
p.add_round_rect(r, radius)
p.add_round_rect(x, y, width height, radius)

// Add circle [13]
p.add_circle(c)
p.add_circle(cx, cy, radius)

// Move current point [14]
p.move_to(pt)
p.move_to(x, y)

// Add an arc [15]
p.arc_to(pt1, pt2, radius)
p.arc_to(x1, y1, x2, y2, radius)

// Add an arc [16]
p.arc(pt, radius, start_angle, end_angle, ccw)
p.arc(pt, radius, start_angle, end_angle)
p.arc(x, y, radius, start_angle, end_angle, ccw)
p.arc(x, y, radius, start_angle, end_angle)

// Add a quadratic Bézier curve [17]
p.quadratic_curve_to(cp, end)
p.quadratic_curve_to(cpx, cpy, x, y)

// Add a cubic Bézier curve [18]
p.bezier_curve_to(cp1, cp2, end)
p.bezier_curve_to(cp1x, cp1y, cp2x, cp2y, x, y)

// Set the fill-rule [19]
p.fill_rule(fr)

```

### Notation

| `r`                               | Instance of `rect`.                  |
| `pt`, `pt1`, `pt2`                | Instance of `point`.                 |
| `cp`, `cp1`, `cp2`, `end`         | Instance of `point`.                 |
| `x`, `y`, `width`, `height`       | Scalar values.                       |
| `radius`, `x1`, `y1`, `x2`, `y2`  | Scalar values.                       |
| `cpx`, `cpy`                      | Scalar values.                       |
| `start_angle`, `end_angle`        | Scalar values.                       |
| `ccw`                             | A Boolean value.                     |
| `c`                               | Instance of `circle`.                |
| `svg_def`                         | Instance of `std::string_view`.      |
| `p`, `p2`                         | Instance of `path`                   |
| `fr`                              | Instance of `path::fill_rule_enum`   |

### Semantics
1. Default construct a `path`.
2. Construct a `path`:
    1. A rectangle from a `rect`, `r`.
    2. A rounded-rectangle from a `rect`, `r`, and `radius`.
    3. A circle from a `circle`, `c`.
    4. A complex path from a string view that conforms to the syntax of an
       [SVG path
       element](https://www.w3.org/TR/SVG11/paths.html#PathElement).
3. Copy construct a `path `, `p`.
4. Assign `p2`, to `p`.
5. Returns `true` if a `p2`, is equal to `p`.
6. Returns `true` if `p2`, is not equal to a `p`.
7. Returns `true` if `p` is empty.
8. Returns `true` if point `pt` (or `x, y`) is inside `p`.
9. Get the smallest rectangle that encapsulates the path, `p`.
10. Close the current shape.
11. Add rectangle, `r` (or `x, y, width height`), to the current path.
12. Add rounded rectangle, `r, radius` (or `x, y, width, height, radius``) to
    the current path.
13. Add circle, `c` (or `cx, cy, radius`) to the current path.
14. Move the current point to `pt` (or `x, y`).
15. Add an arc, to the current path, between two tangents `pt1` and `pt2` (or
    `x1, y1, x2, y2`) with the given `radius`.
16. Add an arc, to the current path, given a center point, `pt` (or `x, y`),
    `start_angle` (in radians) and an `end_angle` and a `bool` flag, `ccw`,
    that specifies whether counterclockwise (flag is `false`) or clockwise
    (flag is 'true`). The default is `false` (clockwise).
17. Add a quadratic Bézier curve to the current path using the specified
    control point, `cp` (or `cpx, cpy`) and ending point`end` (or `x, y`).
    The starting point for the curve is the current point.
18. Add a cubic Bézier curve to the current path using the specified control
    points `cp1` and `cp2` (or `cp1x, cp1y` and `cp2x, cp2y`) and ending
    point`end` (or `x, y`). The starting point for the curve is the current
    point.
19. Set the path's fill rule to `fr` (which can be `path::fill_winding` or
    `path::fill_odd_even`).

-------------------------------------------------------------------------------

*Copyright (c) 2014-2020 Joel de Guzman. All rights reserved.*
*Distributed under the [MIT License](https://opensource.org/licenses/MIT)*
