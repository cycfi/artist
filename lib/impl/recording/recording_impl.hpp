/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]

   Internals shared by the recording backend's translation units.
=============================================================================*/
#if !defined(ARTIST_RECORDING_IMPL_SEPTEMBER_13_2026)
#define ARTIST_RECORDING_IMPL_SEPTEMBER_13_2026

#include <artist/recording.hpp>
#include <artist/path.hpp>
#include <artist/font.hpp>
#include <hb.h>
#include <string_view>
#include <vector>

namespace cycfi::artist::recording
{
   ////////////////////////////////////////////////////////////////////////////
   // A path flattened to polylines as it is built: arcs and curves are
   // sampled, so bounds and hit testing need no geometry library. Points are
   // stored transformed by `xf`, which is the identity for a standalone path
   // and the current transform for the canvas's own path, so that a transform
   // applies at the moment each piece is added, as it does on every backend.
   ////////////////////////////////////////////////////////////////////////////
   struct path_impl
   {
      struct subpath
      {
         std::vector<point>   pts;
         bool                 closed = false;
      };

      std::vector<subpath>    subpaths;
      affine_transform        xf;
      point                   start = {};       // untransformed, for arc_to and curves
      point                   current = {};
      bool                    has_current = false;
      artist::path::fill_rule_enum
                              rule = artist::path::fill_winding;
      std::vector<float>      ops;              // the calls that built it, for equality

      bool                    empty() const     { return subpaths.empty(); }
      bool                    drawable() const;
      void                    clear();
      void                    set_transform(affine_transform const& m);

      void                    move_to(point p);
      void                    line_to(point p);
      void                    close();
      void                    arc(point c, float r, float a0, float a1, bool ccw);
      void                    arc_to(point p1, point p2, float r);
      void                    quad_to(point cp, point end);
      void                    cubic_to(point cp1, point cp2, point end);
      void                    add_rect(rect const& r);
      void                    add_round_rect(rect const& r, float radius);
      void                    append(path_impl const& other);

      rect                    bounds() const;   // in the space the points are stored in
      bool                    includes(point p) const;

   private:

      void                    emit_move(point p);
      void                    emit_line(point p);
      void                    emit_close();
      void                    emit_arc(point c, float r, float a0, float a1, bool ccw);
   };

   ////////////////////////////////////////////////////////////////////////////
   // What an offscreen_image hands to a canvas: where to record, and the
   // surface extent. While a canvas is alive it installs `text_sink`, so a
   // drawer that goes around the canvas API (text_run) records through the
   // canvas's transform and clip.
   ////////////////////////////////////////////////////////////////////////////
   using text_sink_fn = void(*)(void* state, op kind, std::string_view utf8, rect box, color c);

   struct canvas_impl
   {
      journal*                out = nullptr;
      extent                  size = {};
      void*                   state = nullptr;
      text_sink_fn            text_sink = nullptr;
   };

   rect                       transform_bounds(affine_transform const& m, rect const& r);
   rect                       intersect(rect const& a, rect const& b);   // empty when disjoint

   // Record text laid out by a text_run: `box` is in user space.
   void                       record_text(
                                 canvas& cnv, op kind
                               , std::string_view utf8, rect box, color c
                              );

   ////////////////////////////////////////////////////////////////////////////
   // Shaping
   ////////////////////////////////////////////////////////////////////////////
   struct shaped_glyph
   {
      uint32_t                cluster;       // UTF-32 index of its first code point
      float                   x_advance;
      float                   x_offset;
   };

   struct shaped_run
   {
      std::vector<shaped_glyph>  glyphs;
      float                      advance = 0;
   };

   shaped_run                 shape(font const& f, std::u32string_view utf32);
   float                      text_advance(font const& f, std::string_view utf8);
}

namespace cycfi::artist
{
   struct font_impl
   {
                              font_impl() = default;
                              font_impl(font_impl const& rhs);
                              ~font_impl();
      font_impl&              operator=(font_impl const&) = delete;

      hb_font_t*              hb = nullptr;
      float                   size = 0;
      float                   ascent = 0;
      float                   descent = 0;
      float                   leading = 0;
   };
}

#endif
