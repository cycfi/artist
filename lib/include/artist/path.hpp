/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#if !defined(ELEMENTS_PATH_APRIL_8_2020)
#define ELEMENTS_PATH_APRIL_8_2020

#include <artist/rect.hpp>
#include <artist/circle.hpp>
#include <string_view>
#include <cmath>

#if defined(ARTIST_QUARTZ_2D)
using path_impl = struct CGPath;
#endif

namespace cycfi::artist
{
   class path
   {
   public:
                        path();
                        ~path();

                        path(rect r);
                        path(rect r, float radius);
                        path(circle c);
                        path(std::string_view def);
                        path(path const& rhs);
                        path(path&& rhs) = default;

      path&             operator=(path const& rhs);
      path&             operator=(path&& rhs) = default;

      bool              operator==(path const& rhs) const;
      bool              operator!=(path const& rhs) const;
      bool              is_empty() const;
      bool              includes(point p) const;
      rect              bounds() const;

      void              close();

      void              add(rect r);
      void              add(rect r, float radius);
      void              add(circle c);

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

   private:

      path_impl*        _impl;

#if defined(ARTIST_QUARTZ_2D)
      fill_rule_enum    _fill_rule = fill_winding;
#endif
   };

   ////////////////////////////////////////////////////////////////////////////
   // Inlines
   ////////////////////////////////////////////////////////////////////////////
   inline path::path(rect r)
    : path()
   {
      add(r);
   }

   inline path::path(rect r, float radius)
    : path()
   {
      add(r, radius);
   }

   inline path::path(circle c)
    : path()
   {
      add(c);
   }

   inline bool path::operator!=(path const& rhs) const
   {
      return !(*this == rhs);
   }

   inline void path::move_to(float x, float y)
   {
      move_to({ x, y });
   }

   inline void path::line_to(float x, float y)
   {
      line_to({ x, y });
   }

   inline void path::arc_to(
      float x1, float y1,
      float x2, float y2,
      float radius)
   {
      arc_to({ x1, y1 }, { x2, y2 }, radius);
   }

   inline void path::arc(
      float x, float y, float radius,
      float start_angle, float end_angle,
      bool ccw)
   {
      arc({ x, y }, radius, start_angle, end_angle, ccw);
   }

   inline void path::quadratic_curve_to(float cpx, float cpy, float x, float y)
   {
      quadratic_curve_to({ cpx, cpy }, { x, y });
   }

   inline void path::bezier_curve_to(
      float cp1x, float cp1y,
      float cp2x, float cp2y,
      float x, float y)
   {
      bezier_curve_to({ cp1x, cp1y }, { cp2x, cp2y }, { x, y });
   }

#if !defined(ARTIST_SKIA)
   inline void path::add(circle c)
   {
      arc(point{ c.cx, c.cy }, c.radius, 0.0, 2 * M_PI);
   }
#endif

   inline path_impl* path::impl()
   {
      return _impl;
   }

   inline path_impl const* path::impl() const
   {
      return _impl;
   }

#if defined(ARTIST_QUARTZ_2D)
   inline void path::fill_rule(fill_rule_enum rule)
   {
      _fill_rule = rule;
   }
#endif
}

#endif
