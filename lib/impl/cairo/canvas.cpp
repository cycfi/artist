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
         std::size_t path_hash;

         bool operator==(shadow_cache_key const& o) const noexcept
         {
            return sw == o.sw && sh_h == o.sh_h && sigma == o.sigma
                && r == o.r && g == o.g && b == o.b && a == o.a
                && path_hash == o.path_hash;
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
      cairo_matrix_t m;
      cairo_matrix_init(&m, 1.0, sy, sx, 1.0, 0.0, 0.0);
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

      auto composite_shadow(
         cairo_t* cr,
         uint8_t* pixels,
         int sw, int sh_h, int stride,
         double x1, double y1,
         double render_scale, int margin,
         double off_sx, double off_sy)
      {
         // surface = render_scale * (user - (x1 - off)) + margin
         cairo_surface_t* surf = cairo_image_surface_create_for_data(
            pixels, CAIRO_FORMAT_ARGB32, sw, sh_h, stride);
         cairo_save(cr);
         cairo_set_source_surface(cr, surf, 0, 0);
         auto* pat = cairo_get_source(cr);
         cairo_matrix_t pm;
         cairo_matrix_init_identity(&pm);
         cairo_matrix_translate(&pm, margin, margin);
         cairo_matrix_scale(&pm, render_scale, render_scale);
         cairo_matrix_translate(
            &pm, -(x1 + off_sx / render_scale), -(y1 + off_sy / render_scale));
         cairo_pattern_set_matrix(pat, &pm);
         cairo_paint(cr);
         cairo_restore(cr);
         cairo_surface_destroy(surf);
      }

      void draw_shadow(
         cairo_t* cr,
         canvas::canvas_state::shadow_info const& sh,
         canvas::canvas_state& state,
         bool is_fill)
      {
         cairo_path_t* path = cairo_copy_path(cr);
         if (!path || path->status != CAIRO_STATUS_SUCCESS)
         {
            if (path) cairo_path_destroy(path);
            return;
         }

         // Use stroke extents for stroke shadows — cairo_path_extents returns
         // only the geometric path bounds, but a stroke extends ½·line_width
         // beyond them. Sizing the surface from the path extents would let the
         // stroke outer edge eat into the blur margin and clip the glow.
         double x1, y1, x2, y2;
         if (is_fill)
            cairo_path_extents(cr, &x1, &y1, &x2, &y2);
         else
            cairo_stroke_extents(cr, &x1, &y1, &x2, &y2);

         // Derive the CTM scale (user-applied transforms) and the surface device
         // scale (Retina) separately. The shadow surface is rendered at the full
         // physical resolution (render_scale = ctm_scale * device_scale) so the
         // blur is computed at device resolution — never on a low-res user-space
         // bitmap that is then heavily upscaled (which over-softens the blur and
         // widens the feather at large user scales, e.g. cnv.scale(10, 10)).
         cairo_matrix_t ctm;
         cairo_get_matrix(cr, &ctm);
         double ctm_scale = std::sqrt(ctm.xx * ctm.xx + ctm.xy * ctm.xy);
         if (ctm_scale < 0.01) ctm_scale = 1.0;

         double dev_x = 1.0, dev_y = 1.0;
         cairo_surface_get_device_scale(cairo_get_target(cr), &dev_x, &dev_y);
         double device_scale = (dev_x + dev_y) * 0.5;
         if (device_scale < 0.01) device_scale = 1.0;

         double render_scale = ctm_scale * device_scale;
         if (render_scale < 0.01) render_scale = 1.0;

         // sigma in surface pixels. blur is in default-user-space (logical pt);
         // the physical blur scales with device_scale only, matching Quartz —
         // it does NOT grow with cnv.scale(), so the glow stays tight when the
         // shape is magnified. Since the surface is rendered at render_scale and
         // physical = surface * device_scale, sigma_surface = blur*0.5*device_scale.
         float  sigma  = static_cast<float>(sh.blur * 0.5 * device_scale);
         // Margin (surface px) covers the blur's full box-blur support so the
         // feather is never clipped by the surface boundary (see blur_margin).
         int    margin = blur_margin(sigma);

         // Offset is in default-user-space; convert to surface pixels.
         double off_sx = sh.offset.x * device_scale;
         double off_sy = sh.offset.y * device_scale;

         int sw   = static_cast<int>(std::ceil((x2 - x1) * render_scale)) + 2 * margin;
         int sh_h = static_cast<int>(std::ceil((y2 - y1) * render_scale)) + 2 * margin;

         if (sw <= 0 || sh_h <= 0)
         {
            cairo_path_destroy(path);
            return;
         }

         shadow_cache_key cache_key{
            sw, sh_h, sigma,
            static_cast<uint8_t>(sh.c.red   * 255.0f),
            static_cast<uint8_t>(sh.c.green * 255.0f),
            static_cast<uint8_t>(sh.c.blue  * 255.0f),
            static_cast<uint8_t>(sh.c.alpha * 255.0f),
            hash_path(path)
         };

         if (auto* cached = find_shadow_cache(state, cache_key))
         {
            composite_shadow(cr,
               cached->pixels.data(), sw, sh_h, cached->stride,
               x1, y1, render_scale, margin, off_sx, off_sy);
            cairo_append_path(cr, path);
            cairo_path_destroy(path);
            return;
         }

         // Cache miss — render shape and blur.
         auto& sc_buf = state.shadow_scratch;
         int stride = cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32, sw);
         std::size_t surf_size = static_cast<std::size_t>(stride * sh_h);
         std::size_t px_count  = static_cast<std::size_t>(sw * sh_h);
         std::size_t cum_count = static_cast<std::size_t>(std::max(sw, sh_h) + 1);
         if (sc_buf.surf_buf.size() < surf_size) sc_buf.surf_buf.resize(surf_size);
         if (sc_buf.alpha.size()    < px_count)  sc_buf.alpha.resize(px_count);
         if (sc_buf.tmp.size()      < px_count)  sc_buf.tmp.resize(px_count);
         if (sc_buf.cum.size()      < cum_count) sc_buf.cum.resize(cum_count);
         sc_buf.stride = stride;

         // Render the shape into the surface at render_scale resolution. User
         // point (x1, y1) maps to surface (margin, margin).
         std::fill(sc_buf.surf_buf.begin(), sc_buf.surf_buf.begin() + surf_size, 0);
         cairo_surface_t* surf = cairo_image_surface_create_for_data(
            sc_buf.surf_buf.data(), CAIRO_FORMAT_ARGB32, sw, sh_h, stride);
         cairo_t* sc = cairo_create(surf);
         cairo_translate(sc, margin, margin);
         cairo_scale(sc, render_scale, render_scale);
         cairo_translate(sc, -x1, -y1);
         if (!is_fill)
            cairo_set_line_width(sc, cairo_get_line_width(cr));
         cairo_append_path(sc, path);
         cairo_set_source_rgba(sc, sh.c.red, sh.c.green, sh.c.blue, sh.c.alpha);
         if (is_fill) cairo_fill(sc); else cairo_stroke(sc);
         cairo_destroy(sc);

         // Blur alpha-only channel (4× smaller working set than ARGB32).
         cairo_surface_flush(surf);
         if (sh.blur > 0.5f)
         {
            uint8_t* pixels = sc_buf.surf_buf.data();
            alpha_extract(pixels, stride, sc_buf.alpha.data(), sw, sh_h);
            approx_gaussian_blur_1ch(
               sc_buf.alpha.data(), sc_buf.tmp.data(),
               sc_buf.cum.data(), sw, sh_h, sigma);
            auto sr = static_cast<uint8_t>(sh.c.red   * 255.0f);
            auto sg = static_cast<uint8_t>(sh.c.green * 255.0f);
            auto sb = static_cast<uint8_t>(sh.c.blue  * 255.0f);
            shadow_reconstruct(pixels, stride, sc_buf.alpha.data(), sw, sh_h, sr, sg, sb);
            cairo_surface_mark_dirty(surf);
         }
         cairo_surface_destroy(surf);

         // Store blurred pixels in cache before compositing.
         auto& entry = alloc_shadow_cache_entry(
            state, cache_key,
            std::vector<uint8_t>(sc_buf.surf_buf.begin(),
                                 sc_buf.surf_buf.begin() + surf_size),
            stride);

         composite_shadow(cr,
            entry.pixels.data(), sw, sh_h, stride,
            x1, y1, render_scale, margin, off_sx, off_sy);

         cairo_append_path(cr, path);
         cairo_path_destroy(path);
      }
   }

   void canvas::fill()
   {
      if (_state->_info.shadow.active)
         draw_shadow(_context, _state->_info.shadow, *_state, true);
      _state->apply_fill_style();
      cairo_fill(_context);
   }

   void canvas::fill_preserve()
   {
      if (_state->_info.shadow.active)
         draw_shadow(_context, _state->_info.shadow, *_state, true);
      _state->apply_fill_style();
      cairo_fill_preserve(_context);
   }

   void canvas::stroke()
   {
      if (_state->_info.shadow.active)
         draw_shadow(_context, _state->_info.shadow, *_state, false);
      _state->apply_stroke_style();
      cairo_stroke(_context);
   }

   void canvas::stroke_preserve()
   {
      if (_state->_info.shadow.active)
         draw_shadow(_context, _state->_info.shadow, *_state, false);
      _state->apply_stroke_style();
      cairo_stroke_preserve(_context);
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
      cairo_set_miter_limit(_context, limit);
   }

   void canvas::shadow_style(point offset, float blur, color c)
   {
      _state->_info.shadow = {offset, blur, c, true};
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
         if (glyphs.empty()) return;

         if (_state->_info.shadow.active)
         {
            cairo_glyph_path(_context, glyphs.data(), int(glyphs.size()));
            draw_shadow(_context, _state->_info.shadow, *_state, true);
            _state->apply_fill_style();
            cairo_fill(_context);
         }
         else
         {
            _state->apply_fill_style();
            // Quartz CG backend requires current point set before show_glyphs.
            if (!glyphs.empty())
               cairo_move_to(_context, glyphs.front().x, glyphs.front().y);
            cairo_show_glyphs(_context, glyphs.data(), int(glyphs.size()));
         }
      }
      else
      {
         // Fallback: no HarfBuzz font set — use unshaped Cairo text.
         auto str = std::string{utf8.data(), utf8.size()};
         cairo_text_extents_t ext;
         cairo_text_extents(_context, str.c_str(), &ext);
         p = get_text_start(_context, p, _state->_info.align, float(ext.x_advance));
         cairo_move_to(_context, p.x, p.y);
         if (_state->_info.shadow.active)
         {
            cairo_text_path(_context, str.c_str());
            draw_shadow(_context, _state->_info.shadow, *_state, true);
            _state->apply_fill_style();
            cairo_fill(_context);
         }
         else
         {
            _state->apply_fill_style();
            cairo_show_text(_context, str.c_str());
         }
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
      if (!pic.impl() || !pic.impl()->surface) return;
      auto s = new_state();
      auto w = dest.width();
      auto h = dest.height();
      translate(dest.top_left());
      auto sx = w / src.width();
      auto sy = h / src.height();
      scale({sx, sy});
      // Clip to the destination rect before painting. Cairo's unbounded operators
      // (IN, OUT, SOURCE, DEST_IN, XOR, etc.) affect the entire clip region, not
      // just the filled path. Without this clip they would clear the whole surface.
      cairo_rectangle(_context, 0, 0, src.width(), src.height());
      cairo_clip(_context);
      cairo_set_source_surface(_context, pic.impl()->surface, -src.left, -src.top);
      cairo_paint(_context);
   }
}
