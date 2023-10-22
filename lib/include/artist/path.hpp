/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#if !defined(ARTIST_PATH_APRIL_8_2020)
#define ARTIST_PATH_APRIL_8_2020

#include <infra/support.hpp>
#include <artist/rect.hpp>
#include <artist/circle.hpp>
#include <algorithm>
#include <string_view>
#include <cmath>

#if defined(ARTIST_QUARTZ_2D)
using path_impl = struct CGPath;
#elif defined(ARTIST_SKIA)
using path_impl = class SkPath;
#endif

namespace cycfi::artist
{
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

#if defined(ARTIST_QUARTZ_2D)
      fill_rule_enum    fill_rule() const { return _fill_rule; }
#endif

      path_impl*        impl();
      path_impl const*  impl() const;

   private:

      path_impl*        _impl;

      void              add_round_rect_impl(rect const& r, float radius);

#if defined(ARTIST_QUARTZ_2D)
      fill_rule_enum    _fill_rule = fill_winding;
#endif
   };

   ////////////////////////////////////////////////////////////////////////////
   // Inlines
   ////////////////////////////////////////////////////////////////////////////
   inline path::path(rect const& r)
    : path()
   {
      add_rect(r);
   }

   inline path::path(rect const& r, float radius)
    : path()
   {
      add_round_rect(r, radius);
   }

   inline path::path(circle const& c)
    : path()
   {
      add_circle(c);
   }

   inline bool path::operator!=(path const& rhs) const
   {
      return !(*this == rhs);
   }

   inline bool path::includes(float x, float y) const
   {
      return includes({x, y});
   }

   inline void path::add_round_rect(rect const& r, float radius)
   {
      radius = std::clamp(radius, 0.0f, std::min(r.width(), r.height()) / 2);
      add_round_rect_impl(r, radius);
   }

   inline void path::add_rect(float x, float y, float width, float height)
   {
      add_rect({x, y, extent{width, height}});
   }

   inline void path::add_round_rect(
      float x, float y
    , float width, float height
    , float radius
   )
   {
      add_round_rect({x, y, extent{width, height}}, radius);
   }

   inline void path::add_circle(float cx, float cy, float radius)
   {
      add_circle({cx, cy, radius});
   }

   inline void path::move_to(float x, float y)
   {
      move_to({x, y});
   }

   inline void path::line_to(float x, float y)
   {
      line_to({x, y});
   }

   inline void path::arc_to(
      float x1, float y1,
      float x2, float y2,
      float radius)
   {
      arc_to({x1, y1}, {x2, y2}, radius);
   }

   inline void path::arc(
      float x, float y, float radius,
      float start_angle, float end_angle,
      bool ccw)
   {
      arc({x, y}, radius, start_angle, end_angle, ccw);
   }

   inline void path::quadratic_curve_to(float cpx, float cpy, float x, float y)
   {
      quadratic_curve_to({cpx, cpy}, {x, y});
   }

   inline void path::bezier_curve_to(
      float cp1x, float cp1y,
      float cp2x, float cp2y,
      float x, float y)
   {
      bezier_curve_to({cp1x, cp1y}, {cp2x, cp2y}, {x, y});
   }

#if !defined(ARTIST_SKIA)
   inline void path::add_circle(circle const& c)
   {
      arc(point{c.cx, c.cy}, c.radius, 0.0, 2 * pi);
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


// The following may seem like crazy code! It is a workaround over MSVC
// producing bad code if this is defined in the cpp file that causes a really
// odd crash EVEN IF THIS MEMBER FUNCTION IS NEVER EVER CALLED!!! Alas, I
// can't reproduce this compiler bug in a standalone minimal cpp file to file
// a bug report. Happens only in debug mode. It took 2 days to track this.
// What a !@#$ waste of time, Microsoft!!!

#if defined(ARTIST_SKIA) && defined(_WIN32)
#include <SkPath.h>

namespace cycfi::artist
{
   inline rect path::bounds() const
   {
      auto const& r = _impl->getBounds();
      return rect{r.fLeft, r.fTop, r.fRight, r.fBottom};
   }
}

#endif

#endif
