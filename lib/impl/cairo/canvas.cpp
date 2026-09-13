/*=============================================================================
   Copyright (c) 2016-2024 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <artist/canvas.hpp>
#include <artist/image.hpp>
#include "cairo_private.hpp"
#include "cairo_text.hpp"
#include "shadow_blur.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <optional>
#include <stack>
#include <stdexcept>

namespace cycfi::artist
{
   ////////////////////////////////////////////////////////////////////////////
   // canvas_state — manages per-save/restore paint style and text alignment.
   // Styles are stored as deferred lambdas and applied just before each draw.
   class canvas::canvas_state
   {
   public:

      struct shadow_info
      {
         point  offset = {0, 0};
         float  blur   = 0;
         color  c      = colors::black;
         bool   active = false;
      };

      struct info
      {
         enum pattern_state { none_set, stroke_set, fill_set };

         std::function<void()>  stroke_style;
         std::function<void()>  fill_style;
         int                    align       = 0;
         pattern_state          pattern_set = none_set;
         shadow_info            shadow;
         class font             font;       // current font, for HarfBuzz shaping
      };

      void  apply_fill_style();
      void  apply_stroke_style();

      using state_stack = std::stack<info>;

      info        _info;
      state_stack _stack;

      // Inverse of the initial CTM captured at canvas construction.  The host
      // hands us a context whose CTM may already encode a base transform (e.g.
      // the GTK content-area / window-decoration offset).  device_to_user and
      // user_to_device are computed relative to this baseline so that the view
      // origin maps to the host drawing origin (matching the legacy canvas).
      cairo_matrix_t _inv_affine;

      // Reusable scratch buffers for shadow rendering — never shrink, never
      // reallocate unless the shadow surface grows larger than a previous frame.
      struct shadow_scratch_t
      {
         std::vector<uint8_t>  surf_buf;  // ARGB32 pixel data for the temp surface
         std::vector<uint8_t>  alpha;     // single-channel alpha (w*h bytes)
         std::vector<uint8_t>  tmp;       // blur ping-pong / transpose scratch
         std::vector<int32_t>  cum;       // prefix-sum scratch (max(w,h)+1 elements)
         int                   stride = 0;
      } shadow_scratch;

      struct shadow_cache_key
      {
         int         sw, sh_h;
         float       sigma;
         uint8_t     r, g, b, a;
         std::size_t content_hash;
         float       xx, yx, xy, yy, tx, ty;

         bool operator==(shadow_cache_key const& o) const noexcept
         {
            return sw == o.sw && sh_h == o.sh_h && sigma == o.sigma
                && r == o.r && g == o.g && b == o.b && a == o.a
                && content_hash == o.content_hash
                && xx == o.xx && yx == o.yx && xy == o.xy && yy == o.yy
                && tx == o.tx && ty == o.ty;
         }
      };

      struct shadow_cache_entry
      {
         shadow_cache_key     key;
         std::vector<uint8_t> pixels;
         int                  stride;
      };

      static constexpr std::size_t k_shadow_cache_size = 32;
      std::array<std::optional<shadow_cache_entry>, k_shadow_cache_size> shadow_cache;
      std::size_t shadow_cache_next = 0;
   };

   inline void canvas::canvas_state::apply_fill_style()
   {
      if (_info.pattern_set != _info.fill_set && _info.fill_style)
      {
         _info.fill_style();
         _info.pattern_set = _info.fill_set;
      }
   }

   inline void canvas::canvas_state::apply_stroke_style()
   {
      if (_info.pattern_set != _info.stroke_set && _info.stroke_style)
      {
         _info.stroke_style();
         _info.pattern_set = _info.stroke_set;
      }
   }

   ////////////////////////////////////////////////////////////////////////////
   canvas::canvas(canvas_impl* context_)
    : _context{context_}
    , _state{std::make_unique<canvas_state>()}
   {
      cairo_get_matrix(_context, &_state->_inv_affine);
      cairo_matrix_invert(&_state->_inv_affine);

      // The W3C defaults. Cairo's own default line width is 2.
      cairo_set_line_width(_context, 1);
      cairo_set_miter_limit(_context, 10);
   }

   canvas::~canvas()
   {
   }

   ////////////////////////////////////////////////////////////////////////////
   // Transforms

   void canvas::translate(point p)
   {
      cairo_translate(_context, p.x, p.y);
   }

   void canvas::rotate(float rad)
   {
      cairo_rotate(_context, rad);
   }

   void canvas::scale(point p)
   {
      cairo_scale(_context, p.x, p.y);
   }

   void canvas::skew(double sx, double sy)
   {
      // sx and sy are angles, as for affine_transform::skew: sx shears y
      // with x, sy shears x with y.
      cairo_matrix_t m;
      cairo_matrix_init(&m, 1.0, std::tan(sx), std::tan(sy), 1.0, 0.0, 0.0);
      cairo_transform(_context, &m);
   }

   point canvas::device_to_user(point p)
   {
      // Map device->user relative to the initial CTM (see _inv_affine).
      cairo_matrix_t affine;
      cairo_get_matrix(_context, &affine);
      cairo_matrix_t xaf;
      cairo_matrix_multiply(&xaf, &affine, &_state->_inv_affine);
      cairo_matrix_invert(&xaf);
      double x = p.x, y = p.y;
      cairo_matrix_transform_point(&xaf, &x, &y);
      return {float(x), float(y)};
   }

   point canvas::user_to_device(point p)
   {
      // Map user->device relative to the initial CTM (see _inv_affine).
      cairo_matrix_t affine;
      cairo_get_matrix(_context, &affine);
      cairo_matrix_t xaf;
      cairo_matrix_multiply(&xaf, &affine, &_state->_inv_affine);
      double x = p.x, y = p.y;
      cairo_matrix_transform_point(&xaf, &x, &y);
      return {float(x), float(y)};
   }

   // Cairo matrix: {xx=a, yx=b, xy=c, yy=d, x0=tx, y0=ty}
   affine_transform canvas::transform() const
   {
      cairo_matrix_t m;
      cairo_get_matrix(_context, &m);
      return affine_transform{m.xx, m.yx, m.xy, m.yy, m.x0, m.y0};
   }

   void canvas::transform(affine_transform const& mat)
   {
      cairo_matrix_t m{mat.a, mat.b, mat.c, mat.d, mat.tx, mat.ty};
      cairo_set_matrix(_context, &m);
   }

   void canvas::transform(double a, double b, double c, double d, double tx, double ty)
   {
      cairo_matrix_t m{a, b, c, d, tx, ty};
      cairo_set_matrix(_context, &m);
   }

   ////////////////////////////////////////////////////////////////////////////
   // Save / restore

   void canvas::save()
   {
      cairo_save(_context);
      _state->_stack.push(_state->_info);
   }

   void canvas::restore()
   {
      if (!_state->_stack.empty())
      {
         _state->_info = _state->_stack.top();
         _state->_stack.pop();
      }
      cairo_restore(_context);
   }

   ////////////////////////////////////////////////////////////////////////////
   // Paths

   void canvas::begin_path()
   {
      cairo_new_path(_context);
   }

   void canvas::close_path()
   {
      cairo_close_path(_context);
   }

   namespace
   {
      using shadow_scratch    = canvas::canvas_state::shadow_scratch_t;
      using shadow_cache_key   = canvas::canvas_state::shadow_cache_key;
      using shadow_cache_entry = canvas::canvas_state::shadow_cache_entry;
      using shadow_info        = canvas::canvas_state::shadow_info;
      using shadow_render      = std::function<void(cairo_t*)>;

      std::size_t hash_path(cairo_path_t* path) noexcept
      {
         // FNV-1a over raw cairo_path_data_t bytes
         std::size_t h = 14695981039346656037ULL;
         auto const* bytes = reinterpret_cast<uint8_t const*>(path->data);
         std::size_t n = static_cast<std::size_t>(path->num_data)
                         * sizeof(cairo_path_data_t);
         for (std::size_t i = 0; i < n; ++i)
            { h ^= bytes[i]; h *= 1099511628211ULL; }
         return h;
      }

      shadow_cache_entry* find_shadow_cache(canvas::canvas_state& state,
                                             shadow_cache_key const& key) noexcept
      {
         auto it = std::find_if(state.shadow_cache.begin(), state.shadow_cache.end(),
            [&](auto const& e) { return e && e->key == key; });
         return it != state.shadow_cache.end() ? &**it : nullptr;
      }

      shadow_cache_entry& alloc_shadow_cache_entry(canvas::canvas_state& state,
                                                    shadow_cache_key key,
                                                    std::vector<uint8_t> pixels,
                                                    int stride)
      {
         auto& slot = state.shadow_cache[state.shadow_cache_next];
         state.shadow_cache_next =
            (state.shadow_cache_next + 1) % canvas::canvas_state::k_shadow_cache_size;
         slot.emplace(shadow_cache_entry{std::move(key), std::move(pixels), stride});
         return *slot;
      }

      void mix(std::size_t& h, double v) noexcept
      {
         h = (h ^ std::hash<double>{}(v)) * 1099511628211ULL;
      }

      // The device-space bounding box of a user-space rectangle.
      void device_box(
         cairo_matrix_t const& m
       , double& x1, double& y1, double& x2, double& y2)
      {
         double xs[4] = {x1, x2, x2, x1};
         double ys[4] = {y1, y1, y2, y2};
         for (int i = 0; i != 4; ++i)
            cairo_matrix_transform_point(&m, &xs[i], &ys[i]);
         x1 = *std::min_element(xs, xs + 4);
         x2 = *std::max_element(xs, xs + 4);
         y1 = *std::min_element(ys, ys + 4);
         y2 = *std::max_element(ys, ys + 4);
      }

      // Where a shadow is rendered: a bitmap in device space. The offset and
      // blur are in the canvas's initial user space, so the transform in
      // effect does not change them, as in the W3C canvas API.
      struct shadow_frame
      {
         double   x1, y1;        // device-space origin of the bitmap
         double   scale;         // bitmap pixels per device unit
         double   off_x, off_y;  // the offset, in device units
         float    sigma;         // in bitmap pixels
         int      margin;
         int      w, h;
      };

      bool make_shadow_frame(
         cairo_t* cr, canvas::canvas_state const& state
       , double x1, double y1, double x2, double y2
       , shadow_frame& f)
      {
         auto const& sh = state._info.shadow;
         cairo_matrix_t ctm;
         cairo_get_matrix(cr, &ctm);
         device_box(ctm, x1, y1, x2, y2);

         cairo_matrix_t base = state._inv_affine;
         cairo_matrix_invert(&base);
         double base_scale =
            std::sqrt(std::abs(base.xx * base.yy - base.xy * base.yx));
         f.off_x = base.xx * sh.offset.x + base.xy * sh.offset.y;
         f.off_y = base.yx * sh.offset.x + base.yy * sh.offset.y;

         double dev_x = 1, dev_y = 1;
         cairo_surface_get_device_scale(cairo_get_target(cr), &dev_x, &dev_y);
         f.scale = (dev_x + dev_y) / 2;
         if (f.scale < 0.01)
            f.scale = 1;

         // The blur is twice the Gaussian standard deviation.
         f.sigma = float(sh.blur * 0.5 * base_scale * f.scale);
         f.margin = sh.blur > 0? blur_margin(f.sigma) : 1;

         // Only the part whose shadow can land inside the clip is needed.
         double cx1, cy1, cx2, cy2;
         cairo_clip_extents(cr, &cx1, &cy1, &cx2, &cy2);
         device_box(ctm, cx1, cy1, cx2, cy2);
         double reach = f.margin / f.scale;
         x1 = std::max(x1, cx1 - f.off_x - reach);
         y1 = std::max(y1, cy1 - f.off_y - reach);
         x2 = std::min(x2, cx2 - f.off_x + reach);
         y2 = std::min(y2, cy2 - f.off_y + reach);
         if (x2 <= x1 || y2 <= y1)
            return false;

         // Snap to whole pixels, so a moved shape can reuse a cached shadow.
         f.x1 = std::floor(x1 * f.scale) / f.scale;
         f.y1 = std::floor(y1 * f.scale) / f.scale;
         f.w = int(std::ceil((x2 - f.x1) * f.scale)) + 2 * f.margin;
         f.h = int(std::ceil((y2 - f.y1) * f.scale)) + 2 * f.margin;
         return true;
      }

      // Render what is drawn into the frame's bitmap, then turn it into the
      // shadow: its alpha, blurred, scaled by the shadow color's alpha, in
      // the shadow color. The result is in scratch.surf_buf.
      void render_shadow(
         cairo_t* cr, shadow_info const& sh, shadow_frame const& f
       , shadow_scratch& scratch, shadow_render const& render)
      {
         int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, f.w);
         auto surf_size = std::size_t(stride) * f.h;
         auto px_count = std::size_t(f.w) * f.h;
         auto cum_count = std::size_t(std::max(f.w, f.h) + 1);
         if (scratch.surf_buf.size() < surf_size)
            scratch.surf_buf.resize(surf_size);
         if (scratch.alpha.size() < px_count)
            scratch.alpha.resize(px_count);
         if (scratch.tmp.size() < px_count)
            scratch.tmp.resize(px_count);
         if (scratch.cum.size() < cum_count)
            scratch.cum.resize(cum_count);
         scratch.stride = stride;
         auto* pixels = scratch.surf_buf.data();
         std::fill(pixels, pixels + surf_size, 0);

         cairo_matrix_t ctm;
         cairo_get_matrix(cr, &ctm);
         auto* surf = cairo_image_surface_create_for_data(
            pixels, CAIRO_FORMAT_ARGB32, f.w, f.h, stride);
         auto* sc = cairo_create(surf);
         cairo_translate(sc, f.margin, f.margin);
         cairo_scale(sc, f.scale, f.scale);
         cairo_translate(sc, -f.x1, -f.y1);
         cairo_transform(sc, &ctm);
         render(sc);
         cairo_destroy(sc);
         cairo_surface_flush(surf);
         cairo_surface_destroy(surf);

         auto* alpha = scratch.alpha.data();
         alpha_extract(pixels, stride, alpha, f.w, f.h);
         if (f.sigma >= 0.5f)
         {
            approx_gaussian_blur_1ch(
               alpha, scratch.tmp.data(), scratch.cum.data()
             , f.w, f.h, f.sigma);
         }
         if (sh.c.alpha < 1)
         {
            auto k = unsigned(std::lround(sh.c.alpha * 255));
            for (std::size_t i = 0; i != px_count; ++i)
               alpha[i] = uint8_t((alpha[i] * k + 127) / 255);
         }

         auto byte = [](float v) { return uint8_t(std::lround(v * 255)); };
         shadow_reconstruct(
            pixels, stride, alpha, f.w, f.h
          , byte(sh.c.red), byte(sh.c.green), byte(sh.c.blue));
      }

      // Paint a rendered shadow at its offset, with the current operator
      // and clip.
      void paint_shadow(
         cairo_t* cr, uint8_t* pixels, int stride, shadow_frame const& f)
      {
         auto* surf = cairo_image_surface_create_for_data(
            pixels, CAIRO_FORMAT_ARGB32, f.w, f.h, stride);
         cairo_save(cr);
         cairo_identity_matrix(cr);
         cairo_set_source_surface(cr, surf, 0, 0);
         cairo_matrix_t pm;
         cairo_matrix_init_translate(&pm, f.margin, f.margin);
         cairo_matrix_scale(&pm, f.scale, f.scale);
         cairo_matrix_translate(&pm, -(f.x1 + f.off_x), -(f.y1 + f.off_y));
         cairo_pattern_set_matrix(cairo_get_source(cr), &pm);
         cairo_paint(cr);
         cairo_restore(cr);
         cairo_surface_destroy(surf);
      }

      // Draw the shadow of what render draws inside the user-space box.
      // content_hash identifies what is drawn when the shadow may be cached.
      void draw_shadow(
         cairo_t* cr, canvas::canvas_state& state
       , double x1, double y1, double x2, double y2
       , shadow_render const& render, std::size_t const* content_hash)
      {
         auto const& sh = state._info.shadow;
         shadow_frame f;
         if (!make_shadow_frame(cr, state, x1, y1, x2, y2, f))
            return;

         std::optional<shadow_cache_key> key;
         if (content_hash)
         {
            cairo_matrix_t m;
            cairo_get_matrix(cr, &m);
            auto byte = [](float v) { return uint8_t(std::lround(v * 255)); };
            key = shadow_cache_key{
               f.w, f.h, f.sigma
             , byte(sh.c.red), byte(sh.c.green)
             , byte(sh.c.blue), byte(sh.c.alpha)
             , *content_hash
             , float(m.xx), float(m.yx), float(m.xy), float(m.yy)
             , float(m.x0 - f.x1), float(m.y0 - f.y1)
            };
            if (auto* hit = find_shadow_cache(state, *key))
            {
               paint_shadow(cr, hit->pixels.data(), hit->stride, f);
               return;
            }
         }

         auto& scratch = state.shadow_scratch;
         render_shadow(cr, sh, f, scratch, render);
         auto* pixels = scratch.surf_buf.data();
         if (key)
         {
            auto size = std::size_t(scratch.stride) * f.h;
            auto& entry = alloc_shadow_cache_entry(
               state, *key
             , std::vector<uint8_t>(pixels, pixels + size), scratch.stride);
            pixels = entry.pixels.data();
         }
         paint_shadow(cr, pixels, scratch.stride, f);
      }

      void copy_stroke_style(cairo_t* from, cairo_t* to)
      {
         cairo_set_line_width(to, cairo_get_line_width(from));
         cairo_set_line_cap(to, cairo_get_line_cap(from));
         cairo_set_line_join(to, cairo_get_line_join(from));
         cairo_set_miter_limit(to, cairo_get_miter_limit(from));
         if (int n = cairo_get_dash_count(from); n > 0)
         {
            std::vector<double> dashes(n);
            double offset = 0;
            cairo_get_dash(from, dashes.data(), &offset);
            cairo_set_dash(to, dashes.data(), n, offset);
         }
      }

      // The shadow of the current path, filled or stroked with the current
      // source. The path is left as it is.
      void path_shadow(cairo_t* cr, canvas::canvas_state& state, bool is_fill)
      {
         cairo_path_t* path = cairo_copy_path(cr);
         if (!path || path->status != CAIRO_STATUS_SUCCESS)
         {
            if (path)
               cairo_path_destroy(path);
            return;
         }

         double x1, y1, x2, y2;
         if (is_fill)
            cairo_fill_extents(cr, &x1, &y1, &x2, &y2);
         else
            cairo_stroke_extents(cr, &x1, &y1, &x2, &y2);

         auto render = [cr, path, is_fill](cairo_t* sc)
         {
            cairo_append_path(sc, path);
            cairo_set_source(sc, cairo_get_source(cr));
            if (is_fill)
            {
               cairo_set_fill_rule(sc, cairo_get_fill_rule(cr));
               cairo_fill(sc);
            }
            else
            {
               copy_stroke_style(cr, sc);
               cairo_stroke(sc);
            }
         };

         // Only a solid color is cached: a gradient's shadow varies with it.
         double r, g, b, a;
         bool solid = cairo_pattern_get_rgba(
            cairo_get_source(cr), &r, &g, &b, &a) == CAIRO_STATUS_SUCCESS;
         std::size_t h = 0;
         if (solid)
         {
            h = hash_path(path);
            for (double v : {r, g, b, a, double(is_fill)})
               mix(h, v);
            if (is_fill)
            {
               mix(h, cairo_get_fill_rule(cr));
            }
            else
            {
               mix(h, cairo_get_line_width(cr));
               mix(h, cairo_get_line_cap(cr));
               mix(h, cairo_get_line_join(cr));
               mix(h, cairo_get_miter_limit(cr));
               mix(h, cairo_get_dash_count(cr));
            }
         }
         draw_shadow(cr, state, x1, y1, x2, y2, render, solid? &h : nullptr);
         cairo_path_destroy(path);
      }

      bool is_unbounded(cairo_operator_t op)
      {
         return op == CAIRO_OPERATOR_IN || op == CAIRO_OPERATOR_OUT
            || op == CAIRO_OPERATOR_DEST_IN || op == CAIRO_OPERATOR_DEST_ATOP
            || op == CAIRO_OPERATOR_SOURCE;
      }

      // The unbounded operators clear the destination outside what is
      // drawn, as far as the clip. Draw into a group with source-over, then
      // paint the group through the clip with the operator.
      template <typename F>
      void with_operator(cairo_t* cr, canvas::canvas_state& state, F&& draw)
      {
         if (!is_unbounded(cairo_get_operator(cr)))
         {
            draw();
            return;
         }
         cairo_push_group(cr);
         cairo_set_operator(cr, CAIRO_OPERATOR_OVER);
         draw();
         cairo_pop_group_to_source(cr);
         cairo_paint(cr);
         state._info.pattern_set = canvas::canvas_state::info::none_set;
      }
   }

   void canvas::fill()
   {
      _state->apply_fill_style();
      with_operator(_context, *_state, [this]
      {
         if (_state->_info.shadow.active)
            path_shadow(_context, *_state, true);
         cairo_fill(_context);
      });
   }

   void canvas::fill_preserve()
   {
      _state->apply_fill_style();
      with_operator(_context, *_state, [this]
      {
         if (_state->_info.shadow.active)
            path_shadow(_context, *_state, true);
         cairo_fill_preserve(_context);
      });
   }

   void canvas::stroke()
   {
      _state->apply_stroke_style();
      with_operator(_context, *_state, [this]
      {
         if (_state->_info.shadow.active)
            path_shadow(_context, *_state, false);
         cairo_stroke(_context);
      });
   }

   void canvas::stroke_preserve()
   {
      _state->apply_stroke_style();
      with_operator(_context, *_state, [this]
      {
         if (_state->_info.shadow.active)
            path_shadow(_context, *_state, false);
         cairo_stroke_preserve(_context);
      });
   }

   void canvas::clip()
   {
      cairo_clip(_context);
   }

   void canvas::clip(path const& p)
   {
      if (!p.impl()) return;
      auto* cp = cairo_copy_path(p.impl()->ctx);
      if (cp)
      {
         cairo_append_path(_context, cp);
         cairo_path_destroy(cp);
      }
      // Apply the path's fill rule before clipping — Cairo clips with the
      // current fill rule, so odd-even paths need it set on the context.
      cairo_set_fill_rule(_context,
         p.impl()->fill_rule == path::fill_odd_even
            ? CAIRO_FILL_RULE_EVEN_ODD : CAIRO_FILL_RULE_WINDING);
      cairo_clip(_context);
   }

   rect canvas::clip_extent() const
   {
      double x1, y1, x2, y2;
      cairo_clip_extents(_context, &x1, &y1, &x2, &y2);
      return {float(x1), float(y1), float(x2), float(y2)};
   }

   bool canvas::point_in_path(point p) const
   {
      return cairo_in_fill(_context, p.x, p.y);
   }

   rect canvas::fill_extent() const
   {
      double x1, y1, x2, y2;
      cairo_fill_extents(_context, &x1, &y1, &x2, &y2);
      return {float(x1), float(y1), float(x2), float(y2)};
   }

   void canvas::move_to(point p)
   {
      cairo_move_to(_context, p.x, p.y);
   }

   void canvas::line_to(point p)
   {
      cairo_line_to(_context, p.x, p.y);
   }

   void canvas::arc_to(point p1, point p2, float radius)
   {
      // Adapted from http://code.google.com/p/fxcanvas/
      if (radius == 0)
      {
         line_to(p1);
         return;
      }

      double cpx, cpy;
      cairo_get_current_point(_context, &cpx, &cpy);

      auto a1 = cpy - p1.y;
      auto b1 = cpx - p1.x;
      auto a2 = p2.y - p1.y;
      auto b2 = p2.x - p1.x;
      auto mm = std::fabs(a1 * b2 - b1 * a2);

      if (mm < 1.0e-8)
      {
         line_to(p1);
         return;
      }

      auto dd = a1*a1 + b1*b1;
      auto cc = a2*a2 + b2*b2;
      auto tt = a1*a2 + b1*b2;
      auto k1 = radius * std::sqrt(dd) / mm;
      auto k2 = radius * std::sqrt(cc) / mm;
      auto j1 = k1 * tt / dd;
      auto j2 = k2 * tt / cc;
      auto cx = k1*b2 + k2*b1;
      auto cy = k1*a2 + k2*a1;
      auto px = b1*(k2+j1);
      auto py = a1*(k2+j1);
      auto qx = b2*(k1+j2);
      auto qy = a2*(k1+j2);
      auto start_angle = std::atan2(py - cy, px - cx);
      auto end_angle   = std::atan2(qy - cy, qx - cx);
      bool ccw = (b1*a2 > b2*a1);

      arc({float(cx + p1.x), float(cy + p1.y)},
          radius, float(start_angle), float(end_angle), ccw);
   }

   void canvas::arc(
      point p, float radius,
      float start_angle, float end_angle,
      bool ccw
   )
   {
      radius = std::max(radius, 0.0f);
      if (ccw)
         cairo_arc_negative(_context, p.x, p.y, radius, start_angle, end_angle);
      else
         cairo_arc(_context, p.x, p.y, radius, start_angle, end_angle);
   }

   void canvas::add_rect(rect const& r)
   {
      cairo_rectangle(_context, r.left, r.top, r.width(), r.height());
   }

   void canvas::add_path(path const& p)
   {
      if (!p.impl()) return;
      auto* cp = cairo_copy_path(p.impl()->ctx);
      if (cp)
      {
         cairo_append_path(_context, cp);
         cairo_path_destroy(cp);
      }
   }

   void canvas::clear_rect(rect const& r)
   {
      auto s = new_state();
      cairo_set_operator(_context, CAIRO_OPERATOR_CLEAR);
      cairo_rectangle(_context, r.left, r.top, r.width(), r.height());
      cairo_fill(_context);
   }

   void canvas::quadratic_curve_to(point cp, point end)
   {
      double x, y;
      cairo_get_current_point(_context, &x, &y);
      cairo_curve_to(_context,
         2.0/3.0 * cp.x + 1.0/3.0 * x,
         2.0/3.0 * cp.y + 1.0/3.0 * y,
         2.0/3.0 * cp.x + 1.0/3.0 * end.x,
         2.0/3.0 * cp.y + 1.0/3.0 * end.y,
         end.x, end.y
      );
   }

   void canvas::bezier_curve_to(point cp1, point cp2, point end)
   {
      cairo_curve_to(_context, cp1.x, cp1.y, cp2.x, cp2.y, end.x, end.y);
   }

   void canvas::add_round_rect_impl(rect const& r, float radius)
   {
      auto x = r.left, y = r.top, w = r.right, b = r.bottom;
      constexpr auto a = 3.14159265358979323846 / 180.0;
      cairo_new_sub_path(_context);
      cairo_arc(_context, w-radius, y+radius, radius, -90*a,   0*a);
      cairo_arc(_context, w-radius, b-radius, radius,   0*a,  90*a);
      cairo_arc(_context, x+radius, b-radius, radius,  90*a, 180*a);
      cairo_arc(_context, x+radius, y+radius, radius, 180*a, 270*a);
      cairo_close_path(_context);
   }

   ////////////////////////////////////////////////////////////////////////////
   // Styles

   void canvas::fill_style(color c)
   {
      _state->_info.fill_style = [this, c]()
      {
         cairo_set_source_rgba(_context, c.red, c.green, c.blue, c.alpha);
      };
      if (_state->_info.pattern_set == _state->_info.fill_set)
         _state->_info.pattern_set = _state->_info.none_set;
   }

   void canvas::stroke_style(color c)
   {
      _state->_info.stroke_style = [this, c]()
      {
         cairo_set_source_rgba(_context, c.red, c.green, c.blue, c.alpha);
      };
      if (_state->_info.pattern_set == _state->_info.stroke_set)
         _state->_info.pattern_set = _state->_info.none_set;
   }

   void canvas::line_width(float w)
   {
      // Zero, negative, infinite and NaN widths are ignored.
      if (!(w > 0) || !std::isfinite(w))
         return;
      cairo_set_line_width(_context, w);
   }

   void canvas::line_cap(line_cap_enum cap_)
   {
      cairo_line_cap_t cap = CAIRO_LINE_CAP_BUTT;
      switch (cap_)
      {
         case butt:   cap = CAIRO_LINE_CAP_BUTT;   break;
         case round:  cap = CAIRO_LINE_CAP_ROUND;  break;
         case square: cap = CAIRO_LINE_CAP_SQUARE; break;
      }
      cairo_set_line_cap(_context, cap);
   }

   void canvas::line_join(join_enum join_)
   {
      cairo_line_join_t join = CAIRO_LINE_JOIN_MITER;
      switch (join_)
      {
         case bevel_join: join = CAIRO_LINE_JOIN_BEVEL; break;
         case round_join: join = CAIRO_LINE_JOIN_ROUND; break;
         case miter_join: join = CAIRO_LINE_JOIN_MITER; break;
      }
      cairo_set_line_join(_context, join);
   }

   void canvas::miter_limit(float limit)
   {
      // Zero, negative, infinite and NaN limits are ignored.
      if (!(limit > 0) || !std::isfinite(limit))
         return;
      cairo_set_miter_limit(_context, limit);
   }

   void canvas::shadow_style(point offset, float blur, color c)
   {
      // A shadow is drawn only if it can be seen: a color that is not fully
      // transparent, and an offset or a blur.
      bool const visible =
         c.alpha > 0 && (blur > 0 || offset.x != 0 || offset.y != 0);
      _state->_info.shadow = {offset, blur, c, visible};
   }

   void canvas::global_composite_operation(composite_op_enum mode)
   {
      // Porter-Duff ops: directly supported by Cairo.
      // Extended blend modes (MULTIPLY … HSL_LUMINOSITY): require Cairo >= 1.10.
      // lighter: W3C Porter-Duff Plus (additive) — CAIRO_OPERATOR_ADD is correct.
      // darker: W3C PlusDarker = max(0, Cs+Cd-1). Cairo has no native equivalent;
      //   OPERATOR_DARKEN (channel-min) is used as a known approximation.
      cairo_operator_t op;
      switch (mode)
      {
         case source_over:      op = CAIRO_OPERATOR_OVER;            break;
         case source_atop:      op = CAIRO_OPERATOR_ATOP;            break;
         case source_in:        op = CAIRO_OPERATOR_IN;              break;
         case source_out:       op = CAIRO_OPERATOR_OUT;             break;
         case destination_over: op = CAIRO_OPERATOR_DEST_OVER;       break;
         case destination_atop: op = CAIRO_OPERATOR_DEST_ATOP;       break;
         case destination_in:   op = CAIRO_OPERATOR_DEST_IN;         break;
         case destination_out:  op = CAIRO_OPERATOR_DEST_OUT;        break;
         case lighter:          op = CAIRO_OPERATOR_ADD;             break;
         case darker:           op = CAIRO_OPERATOR_DARKEN;          break;
         case copy:             op = CAIRO_OPERATOR_SOURCE;          break;
         case xor_:             op = CAIRO_OPERATOR_XOR;             break;
         // Cairo >= 1.10 extended blend operators:
         case difference:       op = CAIRO_OPERATOR_DIFFERENCE;      break;
         case exclusion:        op = CAIRO_OPERATOR_EXCLUSION;       break;
         case multiply:         op = CAIRO_OPERATOR_MULTIPLY;        break;
         case screen:           op = CAIRO_OPERATOR_SCREEN;          break;
         case color_dodge:      op = CAIRO_OPERATOR_COLOR_DODGE;     break;
         case color_burn:       op = CAIRO_OPERATOR_COLOR_BURN;      break;
         case soft_light:       op = CAIRO_OPERATOR_SOFT_LIGHT;      break;
         case hard_light:       op = CAIRO_OPERATOR_HARD_LIGHT;      break;
         // Cairo >= 1.10 HSL blend operators:
         case hue:              op = CAIRO_OPERATOR_HSL_HUE;         break;
         case saturation:       op = CAIRO_OPERATOR_HSL_SATURATION;  break;
         case color_op:         op = CAIRO_OPERATOR_HSL_COLOR;       break;
         case luminosity:       op = CAIRO_OPERATOR_HSL_LUMINOSITY;  break;
         default:
            throw std::runtime_error{
               "artist cairo backend: unhandled composite_op_enum value"};
      }
      cairo_set_operator(_context, op);
   }

   ////////////////////////////////////////////////////////////////////////////
   // Fill rule

   void canvas::fill_rule(path::fill_rule_enum rule)
   {
      cairo_set_fill_rule(_context,
         rule == path::fill_winding ? CAIRO_FILL_RULE_WINDING : CAIRO_FILL_RULE_EVEN_ODD);
   }

   ////////////////////////////////////////////////////////////////////////////
   // Gradients
   // Pattern lifetime: cairo_set_source takes its own reference, so we call
   // cairo_pattern_destroy immediately after to release the caller's reference.

   namespace
   {
      cairo_pattern_t* make_linear_pattern(canvas::linear_gradient const& gr)
      {
         auto* pat = cairo_pattern_create_linear(
            gr.start.x, gr.start.y, gr.end.x, gr.end.y);
         for (auto const& cs : gr.color_space)
            cairo_pattern_add_color_stop_rgba(
               pat, cs.offset,
               cs.color.red, cs.color.green, cs.color.blue, cs.color.alpha);
         return pat;
      }

      cairo_pattern_t* make_radial_pattern(canvas::radial_gradient const& gr)
      {
         auto* pat = cairo_pattern_create_radial(
            gr.c1.x, gr.c1.y, gr.c1_radius,
            gr.c2.x, gr.c2.y, gr.c2_radius);
         for (auto const& cs : gr.color_space)
            cairo_pattern_add_color_stop_rgba(
               pat, cs.offset,
               cs.color.red, cs.color.green, cs.color.blue, cs.color.alpha);
         return pat;
      }
   }

   void canvas::fill_style(linear_gradient const& gr)
   {
      _state->_info.fill_style = [this, gr]()
      {
         auto* pat = make_linear_pattern(gr);
         cairo_set_source(_context, pat);
         cairo_pattern_destroy(pat);
      };
      if (_state->_info.pattern_set == _state->_info.fill_set)
         _state->_info.pattern_set = _state->_info.none_set;
   }

   void canvas::fill_style(radial_gradient const& gr)
   {
      _state->_info.fill_style = [this, gr]()
      {
         auto* pat = make_radial_pattern(gr);
         cairo_set_source(_context, pat);
         cairo_pattern_destroy(pat);
      };
      if (_state->_info.pattern_set == _state->_info.fill_set)
         _state->_info.pattern_set = _state->_info.none_set;
   }

   void canvas::stroke_style(linear_gradient const& gr)
   {
      _state->_info.stroke_style = [this, gr]()
      {
         auto* pat = make_linear_pattern(gr);
         cairo_set_source(_context, pat);
         cairo_pattern_destroy(pat);
      };
      if (_state->_info.pattern_set == _state->_info.stroke_set)
         _state->_info.pattern_set = _state->_info.none_set;
   }

   void canvas::stroke_style(radial_gradient const& gr)
   {
      _state->_info.stroke_style = [this, gr]()
      {
         auto* pat = make_radial_pattern(gr);
         cairo_set_source(_context, pat);
         cairo_pattern_destroy(pat);
      };
      if (_state->_info.pattern_set == _state->_info.stroke_set)
         _state->_info.pattern_set = _state->_info.none_set;
   }

   ////////////////////////////////////////////////////////////////////////////
   // Font / text — Stage 5: FreeType/Fontconfig-backed font support.

   void canvas::font(class font const& font_)
   {
      if (font_.impl())
      {
         auto* fi = font_.impl();
#ifdef __APPLE__
         // On macOS: CG face (Quartz surface) vs FT scaled font (everything else).
         // CG faces render correctly under the isFlipped=YES Quartz CTM; FT faces
         // fail silently on non-Quartz surfaces. The FT scaled font has
         // HINT_METRICS_OFF baked in so metrics remain consistent for tests.
         if (fi->_face &&
             cairo_surface_get_type(cairo_get_target(_context))
                == CAIRO_SURFACE_TYPE_QUARTZ)
         {
            cairo_set_font_face(_context, fi->_face);
            cairo_set_font_size(_context, fi->_size);
         }
         else if (fi->_scaled_font)
         {
            cairo_set_scaled_font(_context, fi->_scaled_font);
         }
#else
         if (fi->_scaled_font)
            cairo_set_scaled_font(_context, fi->_scaled_font);
#endif
      }
      _state->_info.font = font_;
   }

   namespace
   {
      // Compute the draw origin from the requested point, alignment flags, shaped
      // advance, and font vertical metrics.  advance_x is the HarfBuzz-shaped
      // horizontal advance used for left/center/right alignment.
      point get_text_start(cairo_t* ctx, point p, int align, float advance_x)
      {
         cairo_font_extents_t font_extents;
         cairo_scaled_font_extents(cairo_get_scaled_font(ctx), &font_extents);

         switch (align & 0x3)
         {
            case canvas::text_halign::right:
               p.x -= advance_x;
               break;
            case canvas::text_halign::center:
               p.x -= advance_x / 2;
               break;
            default:
               break;
         }

         switch (align & 0x1C)
         {
            case canvas::text_valign::top:
               p.y += float(font_extents.ascent);
               break;
            case canvas::text_valign::middle:
               p.y += float(font_extents.ascent) / 2 - float(font_extents.descent) / 2;
               break;
            case canvas::text_valign::bottom:
               p.y -= float(font_extents.descent);
               break;
            default:
               break;
         }

         return p;
      }

      // Build a Cairo glyph array from a shaped_run at baseline position (bx, by).
      // HarfBuzz y_offset is positive-upward; Cairo y is positive-downward, so
      // the offset is subtracted.
      std::vector<cairo_glyph_t> make_cairo_glyphs(
         shaped_run const& run, float bx, float by)
      {
         std::vector<cairo_glyph_t> out;
         out.reserve(run.glyphs.size());
         float pen = bx;
         for (auto const& g : run.glyphs)
         {
            out.push_back({g.codepoint,
                           double(pen + g.x_offset),
                           double(by  - g.y_offset)});
            pen += g.x_advance;
         }
         return out;
      }
   }

   void canvas::fill_text(std::string_view utf8, point p)
   {
      auto const* fi = _state->_info.font.impl();
      if (fi && fi->_hb_font)
      {
         auto run = shape_text(fi->_hb_font.get(), fi->_size, utf8);
         p = get_text_start(_context, p, _state->_info.align, run.advance_x);
         auto glyphs = make_cairo_glyphs(run, p.x, p.y);
         if (glyphs.empty())
            return;

         _state->apply_fill_style();
         with_operator(_context, *_state, [&]
         {
            if (_state->_info.shadow.active)
            {
               cairo_glyph_path(_context, glyphs.data(), int(glyphs.size()));
               path_shadow(_context, *_state, true);
               cairo_fill(_context);
            }
            else
            {
               // Quartz CG backend requires the current point set before
               // show_glyphs.
               cairo_move_to(_context, glyphs.front().x, glyphs.front().y);
               cairo_show_glyphs(_context, glyphs.data(), int(glyphs.size()));
            }
         });
      }
      else
      {
         // Fallback: no HarfBuzz font set, so use unshaped Cairo text.
         auto str = std::string{utf8.data(), utf8.size()};
         cairo_text_extents_t ext;
         cairo_text_extents(_context, str.c_str(), &ext);
         p = get_text_start(
            _context, p, _state->_info.align, float(ext.x_advance));

         _state->apply_fill_style();
         with_operator(_context, *_state, [&]
         {
            cairo_move_to(_context, p.x, p.y);
            if (_state->_info.shadow.active)
            {
               cairo_text_path(_context, str.c_str());
               path_shadow(_context, *_state, true);
               cairo_fill(_context);
            }
            else
            {
               cairo_show_text(_context, str.c_str());
            }
         });
      }
   }

   void canvas::stroke_text(std::string_view utf8, point p)
   {
      auto const* fi = _state->_info.font.impl();
      if (fi && fi->_hb_font)
      {
         auto run = shape_text(fi->_hb_font.get(), fi->_size, utf8);
         p = get_text_start(_context, p, _state->_info.align, run.advance_x);
         auto glyphs = make_cairo_glyphs(run, p.x, p.y);
         if (glyphs.empty()) return;
         _state->apply_stroke_style();
         cairo_glyph_path(_context, glyphs.data(), int(glyphs.size()));
         stroke();
      }
      else
      {
         // Fallback: no HarfBuzz font set — use unshaped Cairo text.
         auto str = std::string{utf8.data(), utf8.size()};
         cairo_text_extents_t ext;
         cairo_text_extents(_context, str.c_str(), &ext);
         _state->apply_stroke_style();
         p = get_text_start(_context, p, _state->_info.align, float(ext.x_advance));
         cairo_move_to(_context, p.x, p.y);
         cairo_text_path(_context, str.c_str());
         stroke();
      }
   }

   canvas::text_metrics canvas::measure_text(std::string_view utf8)
   {
      cairo_font_extents_t font_extents;
      cairo_scaled_font_extents(cairo_get_scaled_font(_context), &font_extents);

      float ascent  = float(font_extents.ascent);
      float descent = float(font_extents.descent);
      float leading = float(font_extents.height) - ascent - descent;
      if (leading < 0) leading = 0;

      // Use HarfBuzz shaped advance for width when a font has been set;
      // fall back to Cairo text extents otherwise.
      float width;
      auto const* fi = _state->_info.font.impl();
      if (fi && fi->_hb_font)
         width = shape_text(fi->_hb_font.get(), fi->_size, utf8).advance_x;
      else
      {
         auto str = std::string{utf8.data(), utf8.size()};
         cairo_text_extents_t extents;
         cairo_scaled_font_text_extents(cairo_get_scaled_font(_context),
            str.c_str(), &extents);
         width = float(extents.x_advance);
      }

      return {
         ascent,
         descent,
         leading,
         // size.x = shaped advance width; size.y = line height (ascent + descent)
         {width, ascent + descent}
      };
   }

   void canvas::text_align(int align)
   {
      _state->_info.align = align;
   }

   void canvas::text_align(text_halign align)
   {
      _state->_info.align |= align;
   }

   void canvas::text_baseline(text_valign align)
   {
      _state->_info.align |= align;
   }

   ////////////////////////////////////////////////////////////////////////////
   // Images

   void canvas::draw(image const& pic, rect const& src, rect const& dest)
   {
      if (!pic.impl() || !pic.impl()->surface)
         return;

      auto s = new_state();
      translate(dest.top_left());
      scale({dest.width() / src.width(), dest.height() / src.height()});

      // Paint the image into its own rectangle, leaving the path as it is.
      auto* surface = pic.impl()->surface;
      double const w = src.width(), h = src.height();
      auto paint = [surface, src, w, h](cairo_t* cr)
      {
         auto* saved = cairo_copy_path(cr);
         cairo_save(cr);
         cairo_new_path(cr);
         cairo_rectangle(cr, 0, 0, w, h);
         cairo_clip(cr);
         cairo_set_source_surface(cr, surface, -src.left, -src.top);
         cairo_paint(cr);
         cairo_restore(cr);
         cairo_new_path(cr);
         cairo_append_path(cr, saved);
         cairo_path_destroy(saved);
      };

      with_operator(_context, *_state, [&]
      {
         if (_state->_info.shadow.active)
            draw_shadow(_context, *_state, 0, 0, w, h, paint, nullptr);
         paint(_context);
      });
   }
}
