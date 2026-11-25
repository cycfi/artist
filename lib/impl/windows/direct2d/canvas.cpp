/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]

   Direct2D canvas. Ported from the 2020 `direct2d` branch and adapted to
   develop's full canvas API. Gaps the 2020 branch left (save/restore stack,
   clip, full transforms, composite ops, text) are being filled incrementally;
   see direct2d_backend_plan.md. Brushes are created lazily and rebuilt on
   device loss via context_state::update/discard.
=============================================================================*/
#include <infra/support.hpp>
#include <infra/utf8_utils.hpp>
#include <artist/canvas.hpp>
#include <artist/canvas_layer.hpp>
#include <artist/detail/analytic_shadow.hpp>
#include <artist/detail/font_cache.hpp>
#include "context.hpp"
#include "path_impl.hpp"
#include "font_impl.hpp"
#include <variant>
#include <stack>
#include <vector>
#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>

namespace cycfi::artist
{
   using namespace d2d;

   ////////////////////////////////////////////////////////////////////////////
   // Composite-op mapping (canvas globalCompositeOperation → D2D)
   ////////////////////////////////////////////////////////////////////////////
   namespace
   {
      // The 13 "blend" ops map to the D2D1Blend effect; the rest are Porter-Duff
      // and map to a D2D1_COMPOSITE_MODE applied directly by DrawImage.
      bool composite_is_blend(canvas::composite_op_enum m)
      {
         switch (m)
         {
            case canvas::darker:      case canvas::difference:
            case canvas::exclusion:   case canvas::multiply:
            case canvas::screen:      case canvas::color_dodge:
            case canvas::color_burn:  case canvas::soft_light:
            case canvas::hard_light:  case canvas::hue:
            case canvas::saturation:  case canvas::color_op:
            case canvas::luminosity:
               return true;
            default:
               return false;
         }
      }

      D2D1_COMPOSITE_MODE to_composite_mode(canvas::composite_op_enum m)
      {
         switch (m)
         {
            case canvas::source_over:      return D2D1_COMPOSITE_MODE_SOURCE_OVER;
            case canvas::source_atop:      return D2D1_COMPOSITE_MODE_SOURCE_ATOP;
            case canvas::source_in:        return D2D1_COMPOSITE_MODE_SOURCE_IN;
            case canvas::source_out:       return D2D1_COMPOSITE_MODE_SOURCE_OUT;
            case canvas::destination_over: return D2D1_COMPOSITE_MODE_DESTINATION_OVER;
            case canvas::destination_atop: return D2D1_COMPOSITE_MODE_DESTINATION_ATOP;
            case canvas::destination_in:   return D2D1_COMPOSITE_MODE_DESTINATION_IN;
            case canvas::destination_out:  return D2D1_COMPOSITE_MODE_DESTINATION_OUT;
            case canvas::lighter:          return D2D1_COMPOSITE_MODE_PLUS;
            case canvas::copy:             return D2D1_COMPOSITE_MODE_SOURCE_COPY;
            case canvas::xor_:             return D2D1_COMPOSITE_MODE_XOR;
            default:                       return D2D1_COMPOSITE_MODE_SOURCE_OVER;
         }
      }

      // `darker` -> DARKEN (channel-min) matches Skia/Cairo, not the W3C
      // PlusDarker; see the cross-backend inconsistency note.
      D2D1_BLEND_MODE to_blend_mode(canvas::composite_op_enum m)
      {
         switch (m)
         {
            case canvas::darker:      return D2D1_BLEND_MODE_DARKEN;
            case canvas::difference:  return D2D1_BLEND_MODE_DIFFERENCE;
            case canvas::exclusion:   return D2D1_BLEND_MODE_EXCLUSION;
            case canvas::multiply:    return D2D1_BLEND_MODE_MULTIPLY;
            case canvas::screen:      return D2D1_BLEND_MODE_SCREEN;
            case canvas::color_dodge: return D2D1_BLEND_MODE_COLOR_DODGE;
            case canvas::color_burn:  return D2D1_BLEND_MODE_COLOR_BURN;
            case canvas::soft_light:  return D2D1_BLEND_MODE_SOFT_LIGHT;
            case canvas::hard_light:  return D2D1_BLEND_MODE_HARD_LIGHT;
            case canvas::hue:         return D2D1_BLEND_MODE_HUE;
            case canvas::saturation:  return D2D1_BLEND_MODE_SATURATION;
            case canvas::color_op:    return D2D1_BLEND_MODE_COLOR;
            case canvas::luminosity:  return D2D1_BLEND_MODE_LUMINOSITY;
            default:                  return D2D1_BLEND_MODE_MULTIPLY;
         }
      }

      // Axis-aligned device-space bounding box of a user-space rect under `m`.
      artist::rect device_bounds(matrix2x2f const& m, artist::rect const& r)
      {
         D2D1_POINT_2F c[4] =
         {
            m.TransformPoint({r.left,  r.top}),    m.TransformPoint({r.right, r.top}),
            m.TransformPoint({r.right, r.bottom}), m.TransformPoint({r.left,  r.bottom})
         };
         float minx = c[0].x, maxx = c[0].x, miny = c[0].y, maxy = c[0].y;
         for (int i = 1; i != 4; ++i)
         {
            minx = std::min(minx, c[i].x); maxx = std::max(maxx, c[i].x);
            miny = std::min(miny, c[i].y); maxy = std::max(maxy, c[i].y);
         }
         return {minx, miny, maxx, maxy};
      }
   }

   ////////////////////////////////////////////////////////////////////////////
   // canvas_state
   ////////////////////////////////////////////////////////////////////////////
   class canvas::canvas_state : public context_state
   {
   public:

      using line_cap_enum = canvas::line_cap_enum;
      using join_enum = canvas::join_enum;
      using composite_op_enum = canvas::composite_op_enum;

      using paint_info = std::variant<
         color
       , canvas::linear_gradient
       , canvas::radial_gradient
      >;

      // Savable state (push/pop on save/restore). The current path is NOT here
      // (canvas semantics: the path is independent of the state stack).
      struct info
      {
         matrix2x2f     matrix         = matrix2x2f::Identity();
         paint_info     fill_info      = colors::black;
         paint_info     stroke_info    = colors::black;
         float          line_width     = 1;
         line_cap_enum  line_cap       = line_cap_enum::butt;
         join_enum      join           = join_enum::miter_join;
         float          miter_limit    = 10;
         point          shadow_offset  = {0, 0};
         float          shadow_blur    = 0;
         color          shadow_color   = colors::black;
         float          blur           = 0;
         class font     font           = font_descr{"Segoe UI", 12};
         int            text_align     = canvas::baseline;
         composite_op_enum composite   = canvas::source_over;
         std::size_t    clip_depth     = 0;   // # of clip layers active at save
         artist::path::fill_rule_enum
                        fill_rule      = artist::path::fill_winding;
      };

                        canvas_state();
                        ~canvas_state();

      void              update(render_target& target) override;
      void              discard() override;
      matrix2x2f        current_matrix() const override { return _stack.top().matrix; }

      info&             current()         { return _stack.top(); }
      info const&       current() const    { return _stack.top(); }
      void              save();
      void              restore();

      artist::path&     path()            { return _path; }

      void              set_fill(paint_info const& info);
      void              set_stroke(paint_info const& info);
      brush*            fill_paint(render_target& target);
      brush*            stroke_paint(render_target& target);
      d2d::stroke_style* stroke_style_obj();

      void              fill(context& ctx, bool preserve);
      void              stroke(context& ctx, bool preserve);

      // Draw `render` (which paints the primitive onto the given target at the
      // current matrix) honoring the current global composite op and blur.
      // `user_bounds` is the primitive's bounding rect in user space (the
      // affected region); it is needed unless the op is source-over with no
      // blur.
      using draw_fn = std::function<void(render_target*)>;
      void              composite_draw(context& ctx, artist::rect user_bounds, draw_fn render);

      // Drop-shadow blur. Public so canvas::fill_text / stroke_text can apply the
      // current shadow under text the same way fill()/stroke() do under paths:
      // `render` draws the primitive (in the supplied shadow brush) into an
      // offscreen, which is Gaussian-blurred, offset, and composited onto the
      // main target. Leaves the main transform at identity.
      using render_function = std::function<void(render_target*, brush*, bool)>;
      void              apply_blur(
                           context& ctx, artist::rect bounds
                         , render_function render, bool stroke);
      void              adjust_for_blur(artist::rect& bounds);

      // The shadow of the current path, about to be filled, drawn from its
      // shape where that can be done. False means nothing is drawn.
      bool              shape_shadow(context& ctx);
      brush*            shadow_paint(render_target& target, color c);

      // A shadow is drawn only if it can be seen: a color that is not fully
      // transparent, and an offset or a blur.
      bool              shadow_visible() const
                        {
                           auto const& c = current();
                           return c.shadow_color.alpha > 0 &&
                              (c.shadow_blur > 0 || c.shadow_offset.x != 0
                                 || c.shadow_offset.y != 0);
                        }

      // Bounds are only for the composite intermediate; plain source-over
      // draws straight to the target.
      bool              needs_bounds() const
                        {
                           return current().composite != canvas::source_over
                              || current().blur > 0;
                        }

   public:

      // Clip layers (PushLayer/PopLayer), nested with save/restore.
      void              set_target(render_target* t)   { _rt = t; }
      // Push a clip layer for `geo` (user space) under the current matrix.
      // Takes one ref on geo (released when the layer is popped on restore).
      void              do_clip(geometry* geo)
                        {
                           if (!_rt || !geo)
                           {
                              release(geo);
                              return;
                           }
                           // The layer is sized to the clip geometry, not the
                           // target: an InfiniteRect content bound allocates
                           // and composites a full-target surface per clip.
                           rectf bounds{};
                           geo->GetBounds(nullptr, &bounds);

                           // A device context manages its own layer surfaces
                           // (PushLayer with no ID2D1Layer), which is cheaper
                           // than creating one per clip. Older targets need
                           // the explicit layer.
                           ID2D1Layer* layer = nullptr;
                           device_context* dc = nullptr;
                           if (FAILED(_rt->QueryInterface(&dc)) || !dc)
                           {
                              if (FAILED(_rt->CreateLayer(nullptr, &layer)) || !layer)
                              {
                                 release(geo);
                                 return;
                              }
                           }
                           release(dc);
                           _rt->SetTransform(current().matrix);
                           _rt->PushLayer(
                              D2D1::LayerParameters(
                                 bounds, geo,
                                 D2D1_ANTIALIAS_MODE_PER_PRIMITIVE,
                                 D2D1::IdentityMatrix(), 1.0f, nullptr,
                                 D2D1_LAYER_OPTIONS_NONE),
                              layer);
                           auto const& m = current().matrix;
                           artist::rect user{
                              bounds.left, bounds.top,
                              bounds.right, bounds.bottom};
                           auto device = narrowed(device_bounds(m, user));
                           _clips.push_back(
                              {layer, geo, false, device, m, user});
                        }
      // A group: a layer with no clip, composited at the opacity when it is
      // popped on restore.
      void              do_group(float opacity)
                        {
                           if (!_rt)
                              return;
                           ID2D1Layer* layer = nullptr;
                           device_context* dc = nullptr;
                           if (FAILED(_rt->QueryInterface(&dc)) || !dc)
                           {
                              if (FAILED(_rt->CreateLayer(nullptr, &layer)) || !layer)
                                 return;
                           }
                           release(dc);
                           _rt->SetTransform(current().matrix);
                           _rt->PushLayer(
                              D2D1::LayerParameters(
                                 D2D1::InfiniteRect(), nullptr,
                                 D2D1_ANTIALIAS_MODE_PER_PRIMITIVE,
                                 D2D1::IdentityMatrix(), opacity, nullptr,
                                 D2D1_LAYER_OPTIONS_NONE),
                              layer);
                           auto const device = _clips.empty()
                              ? artist::rect{-1e9f, -1e9f, 1e9f, 1e9f}
                              : _clips.back().device;
                           _clips.push_back({layer, nullptr, false, device
                            , current().matrix
                            , artist::rect{-1e9f, -1e9f, 1e9f, 1e9f}, opacity});
                        }

      // An axis-aligned rectangle clip needs no layer at all.
      void              do_clip(artist::rect const& r)
                        {
                           if (!_rt)
                              return;
                           _rt->SetTransform(current().matrix);
                           _rt->PushAxisAlignedClip(
                              {r.left, r.top, r.right, r.bottom},
                              D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
                           _clips.push_back({
                              nullptr, nullptr, true,
                              narrowed(device_bounds(current().matrix, r)),
                              current().matrix, r});
                        }

      // Direct2D will not read the target back (CopyFromRenderTarget) while
      // a clip or layer is pushed on it. Take the clips off for the read and
      // put them back after, as they were.
      void              suspend_clips()
                        {
                           if (!_rt)
                              return;
                           auto i = _clips.rbegin();
                           for (; i != _clips.rend(); ++i)
                           {
                              if (i->axis_aligned)
                                 _rt->PopAxisAlignedClip();
                              else
                                 _rt->PopLayer();
                           }
                        }

      void              resume_clips()
                        {
                           if (!_rt)
                              return;
                           for (auto const& c : _clips)
                           {
                              auto const& b = c.bounds;
                              rectf r{b.left, b.top, b.right, b.bottom};
                              auto const aa = D2D1_ANTIALIAS_MODE_PER_PRIMITIVE;
                              _rt->SetTransform(c.matrix);
                              if (c.axis_aligned)
                              {
                                 _rt->PushAxisAlignedClip(r, aa);
                              }
                              else
                              {
                                 _rt->PushLayer(
                                    D2D1::LayerParameters(
                                       c.geo? r : D2D1::InfiniteRect(),
                                       c.geo, aa,
                                       D2D1::IdentityMatrix(), c.opacity,
                                       nullptr, D2D1_LAYER_OPTIONS_NONE),
                                    c.layer);
                              }
                           }
                        }
      std::size_t       clip_count() const             { return _clips.size(); }
      // The active clip's device-space bounds, or none when nothing clips.
      artist::rect const* device_clip() const
                        {
                           // Groups do not clip: the clip is the last real
                           // one below them, if any.
                           for (auto i = _clips.rbegin(); i != _clips.rend(); ++i)
                              if (i->axis_aligned || i->geo)
                                 return &i->device;
                           return nullptr;
                        }
      void              pop_clips_to(std::size_t depth)
                        {
                           while (_clips.size() > depth)
                           {
                              auto c = _clips.back();
                              _clips.pop_back();
                              if (_rt)
                              {
                                 if (c.axis_aligned)
                                    _rt->PopAxisAlignedClip();
                                 else
                                    _rt->PopLayer();
                              }
                              release(c.layer);
                              release(c.geo);
                           }
                        }

   private:

      // Each clip carries the device-space bounds of everything clipped so
      // far (its own bounds intersected with the one below), so the active
      // clip is always the back of the stack.
      struct clip_t
      {
         ID2D1Layer*    layer;
         geometry*      geo;
         bool           axis_aligned;
         artist::rect   device;
         matrix2x2f     matrix;     // as pushed, to push it again
         artist::rect   bounds;     // user space, as pushed
         float          opacity = 1.0f;   // a group's; a group has no geo
      };

      artist::rect      narrowed(artist::rect const& device_bounds_) const
                        {
                           if (_clips.empty())
                              return device_bounds_;
                           return artist::intersection(_clips.back().device, device_bounds_);
                        }

      std::stack<info>  _stack;
      artist::path      _path;
      render_target*    _rt = nullptr;
      std::vector<clip_t> _clips;

      brush*            _fill_paint = nullptr;     // lazy, derived from fill_info
      brush*            _stroke_paint = nullptr;   // lazy, derived from stroke_info
      brush*            _shadow_paint = nullptr;   // lazy, a solid brush
      bool              _fill_solid = false;       // _fill_paint is a solid brush
      bool              _stroke_solid = false;     // _stroke_paint is a solid brush
      bool              _fill_stale = false;       // info changed under the brush (restore)
      bool              _stroke_stale = false;

      // Gradient brushes, keyed by their colour stops. A gradient brush is a
      // stop collection plus a brush, two device objects per creation, and
      // widgets create the same few theme gradients over and over. The stops
      // are what cost; the geometry (points, radii) is set on the cached
      // brush. Small and most-recent-first, so lookups stay cheap.
      struct gradient_entry
      {
         std::vector<color_stop> stops;
         bool                    linear;
         brush*                  paint;
         render_target*          owner;
         float                   start;   // radial_start, for radials
      };
      std::vector<gradient_entry> _gradients;
      brush*            gradient_paint(paint_info const& info, render_target& target);
      void              clear_gradients();
      line_cap_enum     _style_cap = line_cap_enum::butt;   // what _stroke_style was built from
      join_enum         _style_join = join_enum::miter_join;
      float             _style_miter = 10;
      d2d::stroke_style*_stroke_style = nullptr;   // lazy, derived from cap/join/miter
   };

   canvas::canvas_state::canvas_state()
   {
      _stack.push(info{});
   }

   canvas::canvas_state::~canvas_state()
   {
      pop_clips_to(0);   // balance any outstanding PushLayer before EndDraw
      release(_fill_paint);
      release(_stroke_paint);
      release(_shadow_paint);
      release(_stroke_style);
      clear_gradients();
   }

   void canvas::canvas_state::clear_gradients()
   {
      for (auto& g : _gradients)
         release(g.paint);
      _gradients.clear();
   }

   brush* canvas::canvas_state::gradient_paint(paint_info const& info, render_target& target)
   {
      auto const* lg = std::get_if<canvas::linear_gradient>(&info);
      auto const* rg = std::get_if<canvas::radial_gradient>(&info);
      auto const& stops = lg? lg->color_space : rg->color_space;
      bool linear = lg != nullptr;
      float start = rg? radial_start(*rg) : 0.0f;

      // A gradient with no stops, or two identical radial circles, paints
      // nothing, as the W3C API specifies. Direct2D would otherwise build a
      // brush on an empty stop collection.
      if (stops.empty() || (rg && rg->c1 == rg->c2 && rg->c1_radius == rg->c2_radius))
         return make_paint(color{0, 0, 0, 0}, target);

      auto same = [&](gradient_entry const& e)
      {
         if (e.owner != &target || e.linear != linear || e.stops.size() != stops.size()
            || e.start != start)
            return false;
         for (std::size_t i = 0; i != stops.size(); ++i)
            if (e.stops[i].offset != stops[i].offset
               || e.stops[i].color.red != stops[i].color.red
               || e.stops[i].color.green != stops[i].color.green
               || e.stops[i].color.blue != stops[i].color.blue
               || e.stops[i].color.alpha != stops[i].color.alpha)
               return false;
         return true;
      };

      auto it = std::find_if(_gradients.begin(), _gradients.end(), same);
      if (it == _gradients.end())
      {
         constexpr std::size_t cap = 16;
         if (_gradients.size() == cap)
         {
            release(_gradients.back().paint);
            _gradients.pop_back();
         }
         auto paint = lg? make_paint(*lg, target) : make_paint(*rg, target);
         _gradients.insert(_gradients.begin(), {stops, linear, paint, &target, start});
         it = _gradients.begin();
      }
      else if (it != _gradients.begin())
      {
         std::rotate(_gradients.begin(), it, it + 1);   // most recent first
         it = _gradients.begin();
      }

      if (lg)
      {
         auto b = static_cast<linear_gradient_brush*>(it->paint);
         b->SetStartPoint(D2D1::Point2F(lg->start.x, lg->start.y));
         b->SetEndPoint(D2D1::Point2F(lg->end.x, lg->end.y));
      }
      else
      {
         auto b = static_cast<radial_gradient_brush*>(it->paint);
         b->SetCenter(D2D1::Point2F(rg->c2.x, rg->c2.y));
         b->SetGradientOriginOffset(D2D1::Point2F(rg->c1.x - rg->c2.x, rg->c1.y - rg->c2.y));
         b->SetRadiusX(rg->c2_radius);
         b->SetRadiusY(rg->c2_radius);
      }
      it->paint->AddRef();   // the caller's _fill_paint/_stroke_paint holds its own ref
      return it->paint;
   }

   void canvas::canvas_state::update(render_target& /*target*/)
   {
      // Device (re)acquired: drop derived device-dependent resources so they
      // rebuild lazily against the new target.
      discard();
   }

   void canvas::canvas_state::discard()
   {
      release(_fill_paint);
      release(_stroke_paint);
      release(_shadow_paint);
      release(_stroke_style);
      clear_gradients();
   }

   void canvas::canvas_state::save()
   {
      auto copy = _stack.top();
      copy.clip_depth = _clips.size();   // clips active when this level began
      _stack.push(copy);
   }

   void canvas::canvas_state::restore()
   {
      if (_stack.size() > 1)
      {
         pop_clips_to(_stack.top().clip_depth);   // drop clips added this level
         _stack.pop();
         // Derived resources depend on the (now restored) info. They are kept
         // and checked against it on next use rather than rebuilt: every
         // widget saves and restores, so releasing here would recreate a
         // brush and a stroke style per widget.
         _fill_stale = _stroke_stale = true;
      }
   }

   // A solid brush takes a new colour in place. Widgets change fill and stroke
   // colours before nearly every shape, and creating a brush per change is a
   // COM allocation each time; recolouring the one we have is not.
   static bool recolor(brush* paint, bool solid, color const* c)
   {
      if (!paint || !solid || !c)
         return false;
      static_cast<solid_color_brush*>(paint)->SetColor(
         D2D1::ColorF(c->red, c->green, c->blue, c->alpha));
      return true;
   }

   void canvas::canvas_state::set_fill(paint_info const& info)
   {
      current().fill_info = info;
      if (!recolor(_fill_paint, _fill_solid, std::get_if<color>(&info)))
         release(_fill_paint);
   }

   void canvas::canvas_state::set_stroke(paint_info const& info)
   {
      current().stroke_info = info;
      if (!recolor(_stroke_paint, _stroke_solid, std::get_if<color>(&info)))
         release(_stroke_paint);
   }

   brush* canvas::canvas_state::fill_paint(render_target& target)
   {
      if (_fill_stale)
      {
         _fill_stale = false;
         if (!recolor(_fill_paint, _fill_solid, std::get_if<color>(&current().fill_info)))
            release(_fill_paint);
      }
      if (!_fill_paint)
      {
         auto const& info = current().fill_info;
         _fill_solid = std::holds_alternative<color>(info);
         _fill_paint = _fill_solid?
            make_paint(std::get<color>(info), target) : gradient_paint(info, target);
      }
      return _fill_paint;
   }

   brush* canvas::canvas_state::stroke_paint(render_target& target)
   {
      if (_stroke_stale)
      {
         _stroke_stale = false;
         if (!recolor(_stroke_paint, _stroke_solid, std::get_if<color>(&current().stroke_info)))
            release(_stroke_paint);
      }
      if (!_stroke_paint)
      {
         auto const& info = current().stroke_info;
         _stroke_solid = std::holds_alternative<color>(info);
         _stroke_paint = _stroke_solid?
            make_paint(std::get<color>(info), target) : gradient_paint(info, target);
      }
      return _stroke_paint;
   }

   d2d::stroke_style* canvas::canvas_state::stroke_style_obj()
   {
      // Rebuilt only when cap, join or miter limit actually changed.
      auto const& c = current();
      if (_stroke_style
         && (_style_cap != c.line_cap || _style_join != c.join || _style_miter != c.miter_limit))
         release(_stroke_style);
      if (!_stroke_style)
      {
         _stroke_style = make_stroke_style(c.line_cap, c.join, c.miter_limit);
         _style_cap = c.line_cap;
         _style_join = c.join;
         _style_miter = c.miter_limit;
      }
      return _stroke_style;
   }

   void canvas::canvas_state::adjust_for_blur(artist::rect& bounds)
   {
      // The blur is in the canvas's initial units, whatever the transform.
      float offset = current().shadow_blur;
      bounds.left -= offset;
      bounds.right += offset;
      bounds.top -= offset;
      bounds.bottom += offset;
   }

   void canvas::canvas_state::apply_blur(
      context& ctx, artist::rect bounds, render_function render
    , bool stroke)
   {
      ctx.resume();
      auto& cur = current();

      // The blurred shadow must be composited onto the MAIN target, so we need
      // the main target as an ID2D1DeviceContext (D2D 1.1). If unavailable,
      // skip the shadow rather than mis-render.
      device_context* dc = nullptr;
      if (FAILED(ctx.target()->QueryInterface(&dc)) || !dc)
         return;

      // The blur is twice the Gaussian standard deviation, as in the W3C
      // canvas API, and the transform in effect does not change it. With a
      // blur style as well, the shadow is cast from what the blur leaves: the
      // Gaussian of a Gaussian, whose deviations add in squares.
      auto const sigma = std::hypot(cur.shadow_blur / 2, cur.blur);

      // The offscreen covers only what casts the shadow: the primitive's
      // device-space bounds (`bounds` is in user space), cut to the part whose
      // shadow can reach the target. Empty bounds (text, images) mean the
      // whole target. An offscreen as large as the target for every shadow
      // made each shadowed draw allocate a target-sized texture, and Direct2D
      // pools those per size: hundreds of MB once a window has been resized a
      // few times.
      auto main = ctx.target();
      auto const& offset = cur.shadow_offset;
      auto const target_size = main->GetSize();
      float const reach = 3 * sigma;
      artist::rect area{
         -offset.x - reach, -offset.y - reach
       , target_size.width - offset.x + reach, target_size.height - offset.y + reach
      };
      if (bounds.right > bounds.left && bounds.bottom > bounds.top)
      {
         auto db = device_bounds(cur.matrix, bounds);
         area = {
            std::max(area.left, db.left), std::max(area.top, db.top)
          , std::min(area.right, db.right), std::min(area.bottom, db.bottom)
         };
      }

      // Whole device pixels, so the shadow is not resampled when it is placed.
      FLOAT dpi_x = 96.0f, dpi_y = 96.0f;
      main->GetDpi(&dpi_x, &dpi_y);
      float const sx = dpi_x / 96.0f, sy = dpi_y / 96.0f;
      float const x0 = std::floor(area.left * sx) / sx;
      float const y0 = std::floor(area.top * sy) / sy;
      float const x1 = std::ceil(area.right * sx) / sx;
      float const y1 = std::ceil(area.bottom * sy) / sy;
      if (x1 <= x0 || y1 <= y0)
      {
         release(dc);
         return;
      }

      // Render what is drawn, in its own paint, into a compatible offscreen
      // bitmap positioned exactly like it (same transform, less the area's
      // origin): its alpha, point by point, is what casts the shadow.
      brush* paint = stroke? stroke_paint(*main) : fill_paint(*main);
      offscreen_context offscreen{extent{x1 - x0, y1 - y0}, ctx};
      auto bm_target = offscreen.target();
      if (!bm_target)
      {
         release(dc);
         return;
      }
      bm_target->BeginDraw();
      bm_target->Clear(D2D1::ColorF(0, 0, 0, 0));
      bm_target->SetTransform(cur.matrix * D2D1::Matrix3x2F::Translation(-x0, -y0));
      render(bm_target, paint, true);
      bm_target->EndDraw();

      // The shadow effect takes the source's alpha, blurs it and fills it with
      // the shadow color, alpha included. Offset it and draw it onto the main
      // target, under what the caller paints next.
      effect* shadow = nullptr;
      effect* xform = nullptr;
      if (SUCCEEDED(dc->CreateEffect(CLSID_D2D1Shadow, &shadow)) && shadow &&
          SUCCEEDED(dc->CreateEffect(CLSID_D2D12DAffineTransform, &xform)) && xform)
      {
         auto const& c = cur.shadow_color;
         shadow->SetInput(0, offscreen.bitmap());
         shadow->SetValue(D2D1_SHADOW_PROP_BLUR_STANDARD_DEVIATION, sigma);
         shadow->SetValue(
            D2D1_SHADOW_PROP_COLOR
          , D2D1::Vector4F(c.red, c.green, c.blue, c.alpha));
         shadow->SetValue(
            D2D1_SHADOW_PROP_OPTIMIZATION, D2D1_SHADOW_OPTIMIZATION_BALANCED);

         xform->SetInputEffect(0, shadow);
         xform->SetValue(
            D2D1_2DAFFINETRANSFORM_PROP_TRANSFORM_MATRIX,
            D2D1::Matrix3x2F::Translation(x0 + offset.x, y0 + offset.y));

         dc->SetTransform(D2D1::Matrix3x2F::Identity());
         dc->DrawImage(xform, D2D1_INTERPOLATION_MODE_LINEAR);
      }

      release(shadow);
      release(xform);
      release(dc);
   }

   namespace
   {
      using detail::pixel_box;
      using detail::shadow_layout;
      using detail::shadow_mask;
      using detail::shadow_piece;
      using detail::shadow_shape;

      // The alpha of a fill that has one alpha all over: a solid color, or
      // a gradient that covers the plane and has only opaque stops.
      std::optional<double> uniform_alpha(
         canvas::canvas_state::paint_info const& paint)
      {
         if (auto const* c = std::get_if<color>(&paint))
            return c->alpha;

         auto opaque = [](std::vector<canvas::color_stop> const& stops)
         {
            auto solid = [](auto const& s) { return s.color.alpha >= 1; };
            return !stops.empty()
               && std::all_of(stops.begin(), stops.end(), solid);
         };

         using linear = canvas::linear_gradient;
         using radial = canvas::radial_gradient;
         bool covers = false;
         if (auto const* lg = std::get_if<linear>(&paint))
         {
            covers = opaque(lg->color_space);
         }
         else if (auto const* rg = std::get_if<radial>(&paint))
         {
            // A radial one covers the plane when one circle holds the other.
            double const apart =
               std::hypot(rg->c2.x - rg->c1.x, rg->c2.y - rg->c1.y);
            covers = opaque(rg->color_space)
               && apart + std::min(rg->c1_radius, rg->c2_radius)
                  < std::max(rg->c1_radius, rg->c2_radius);
         }
         return covers? std::optional<double>{1.0} : std::nullopt;
      }

      // The path in the target's pixels, px of them to the transform's
      // unit, where it is one rectangle, rounded rectangle or circle that
      // the transform leaves upright or on its side, with round corners
      // still round.
      bool device_shape(
         path_impl const& path, matrix2x2f const& m, double px
       , shadow_shape& s)
      {
         artist::rect r;
         float radius = 0;
         bool const shape = path.shape_primitive(r, radius)
            && r.left < r.right && r.top < r.bottom;
         if (!shape)
            return false;

         double const sx = std::hypot(m._11, m._12);
         double const sy = std::hypot(m._21, m._22);
         auto tiny = [limit = 1e-6 * std::max(sx, sy)](double v)
         {
            return std::abs(v) <= limit;
         };
         bool const upright = tiny(m._12) && tiny(m._21);
         bool const sideways = tiny(m._11) && tiny(m._22);
         bool const even = std::abs(sx - sy) <= 1e-3 * sx;
         if (!(upright || sideways) || (radius > 0 && !even))
            return false;

         auto const b = device_bounds(m, r);
         double const x0 = b.left * px, y0 = b.top * px;
         double const x1 = b.right * px, y1 = b.bottom * px;
         s = {x0, y0, x1, y1
          , std::min({radius * sx * px, (x1 - x0) / 2, (y1 - y0) / 2})};
         return x0 < x1 && y0 < y1;
      }

      // A mask as a bitmap of alpha only, kept with the mask. A bitmap
      // can be drawn on any target of the device it was made on. The
      // device is held, so that no other can take its address.
      struct mask_bitmap
      {
                        ~mask_bitmap()
                        {
                           release(image);
                           release(owner);
                        }

         bitmap*        image = nullptr;
         ID2D1Device*   owner = nullptr;
      };

      bitmap* bitmap_of(
         shadow_mask const& m, render_target& target, ID2D1Device* device)
      {
         auto* kept = static_cast<mask_bitmap*>(m.native.get());
         if (kept && kept->owner == device)
            return kept->image;

         auto made = std::make_shared<mask_bitmap>();
         auto const props = D2D1::BitmapProperties(
            D2D1::PixelFormat(
               DXGI_FORMAT_A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
         auto const hr = target.CreateBitmap(
            D2D1::SizeU(UINT32(m.width), UINT32(m.height))
          , m.alpha.data(), UINT32(m.stride), &props, &made->image);
         if (FAILED(hr) || !made->image)
            return nullptr;
         made->owner = device;
         device->AddRef();
         m.native = made;
         return made->image;
      }

      bool overlap(pixel_box const& a, pixel_box const& b)
      {
         return a.x0 < b.x1 && b.x0 < a.x1 && a.y0 < b.y1 && b.y0 < a.y1;
      }

      // The blurred shadow of s through the brush, in the pieces of l, but
      // for what hide covers or clip leaves out. The target's units are
      // pixels and it is not antialiasing, as an opacity mask requires.
      void draw_pieces(
         render_target& target, ID2D1Device* device, brush* paint
       , shadow_shape const& s, double sigma, shadow_layout const& l
       , pixel_box const& hide, pixel_box const& clip)
      {
         detail::for_each_piece(l, [&](shadow_piece kind, pixel_box const& b)
         {
            if (!overlap(b, clip))
               return;

            // A mask is drawn whole. A line or a fill is the same all
            // along, so only what shows of it is drawn.
            pixel_box seen[4] = {b};
            int const n = kind == shadow_piece::mask
               ? 1 : detail::visible_parts(b, hide, seen);
            if (n == 0)
               return;

            bool const down = kind == shadow_piece::line_down;
            bool const across = kind == shadow_piece::line_across;
            bitmap* image = nullptr;
            if (kind != shadow_piece::fill)
            {
               auto const& m = detail::shadow_mask_of(
                  s, sigma, b.x0, b.y0
                , down? 1 : b.x1 - b.x0, across? 1 : b.y1 - b.y0);
               image = bitmap_of(m, target, device);
               if (!image)
                  return;
            }

            for (int i = 0; i != n; ++i)
            {
               auto const& v = seen[i];
               auto const dest = D2D1::RectF(
                  FLOAT(v.x0), FLOAT(v.y0), FLOAT(v.x1), FLOAT(v.y1));
               if (!image)
               {
                  target.FillRectangle(dest, paint);
                  continue;
               }
               // The one pixel across a line is stretched along it.
               auto const src = D2D1::RectF(
                  FLOAT(down? 0 : v.x0 - b.x0)
                , FLOAT(across? 0 : v.y0 - b.y0)
                , FLOAT(down? 1 : v.x1 - b.x0)
                , FLOAT(across? 1 : v.y1 - b.y0));
               target.FillOpacityMask(
                  image, paint, D2D1_OPACITY_MASK_CONTENT_GRAPHICS
                , &dest, &src);
            }
         });
      }

      // The pixels of a target that a clip in its units leaves, if any.
      pixel_box clip_pixels(
         render_target& target, artist::rect const* clip, double px)
      {
         auto const size = target.GetPixelSize();
         int const w = int(size.width), h = int(size.height);
         if (!clip)
            return {0, 0, w, h};

         auto within = [](double v, int most)
         {
            return int(std::clamp(v, 0.0, double(most)));
         };
         return {
            within(std::floor(clip->left * px), w)
          , within(std::floor(clip->top * px), h)
          , within(std::ceil(clip->right * px), w)
          , within(std::ceil(clip->bottom * px), h)
         };
      }

      // How many pixels a target has to its unit: 0 if not as many down
      // as across.
      double pixel_scale(render_target& target)
      {
         FLOAT dpi_x = 96.0f, dpi_y = 96.0f;
         target.GetDpi(&dpi_x, &dpi_y);
         return dpi_x == dpi_y? dpi_x / 96.0 : 0;
      }

      // The device behind a target, if it has one, for the caller to
      // release.
      ID2D1Device* device_of(render_target& target)
      {
         device_context* dc = nullptr;
         ID2D1Device* device = nullptr;
         if (SUCCEEDED(target.QueryInterface(&dc)) && dc)
            dc->GetDevice(&device);
         release(dc);
         return device;
      }
   }

   brush* canvas::canvas_state::shadow_paint(render_target& target, color c)
   {
      if (!recolor(_shadow_paint, true, &c))
         _shadow_paint = make_paint(c, target);
      return _shadow_paint;
   }

   // Draw the shadow of the current path, about to be filled, where it can
   // be worked out from the shape instead of blurred from its pixels: a
   // rectangle, rounded rectangle or circle on its own, upright, filled
   // with one alpha all over and drawn source-over with no blur of its
   // own. See artist/detail/analytic_shadow.hpp. False means it can't be,
   // and nothing is drawn.
   bool canvas::canvas_state::shape_shadow(context& ctx)
   {
      auto const& cur = current();
      if (cur.composite != canvas::source_over || cur.blur > 0)
         return false;
      auto const alpha = uniform_alpha(cur.fill_info);
      if (!alpha)
         return false;

      ctx.resume();
      auto main = ctx.target();
      double const px = pixel_scale(*main);
      shadow_shape shape;
      if (px <= 0 || !device_shape(*_path.impl(), cur.matrix, px, shape))
         return false;

      // The shadow's offset and blur are in the target's units.
      auto const& offset = cur.shadow_offset;
      double const dx = offset.x * px, dy = offset.y * px;
      double const sigma = cur.shadow_blur / 2 * px;
      double const strength = cur.shadow_color.alpha * *alpha;
      if (strength * 255 < 0.5)
         return true;   // too faint to show

      auto c = cur.shadow_color;
      c.alpha = float(strength);
      if (sigma < 0.5)
      {
         // With no blur, the shadow is the shape in the shadow's color.
         main->SetTransform(
            cur.matrix * D2D1::Matrix3x2F::Translation(offset.x, offset.y));
         _path.impl()->fill(*main, shadow_paint(*main, c), true);
         return true;
      }

      shadow_shape const shadow{
         shape.x0 + dx, shape.y0 + dy, shape.x1 + dx, shape.y1 + dy
       , shape.radius
      };
      auto const l = detail::layout_shadow(shadow, sigma, strength);
      auto* device = l.nx != 0? device_of(*main) : nullptr;
      if (!device)
         return false;

      pixel_box hide;
      if (*alpha >= 1)
         hide = detail::hidden_part(shape);
      auto const aa = main->GetAntialiasMode();
      main->SetTransform(
         D2D1::Matrix3x2F::Scale(FLOAT(1 / px), FLOAT(1 / px)));
      main->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);
      draw_pieces(
         *main, device, shadow_paint(*main, c), shadow, sigma, l, hide
       , clip_pixels(*main, device_clip(), px));
      main->SetAntialiasMode(aa);
      release(device);
      return true;
   }

   namespace
   {
      bool is_unbounded(canvas::composite_op_enum m)
      {
         return m == canvas::source_in || m == canvas::source_out
            || m == canvas::destination_in || m == canvas::destination_atop
            || m == canvas::copy;
      }
   }

   void canvas::canvas_state::composite_draw(
      context& ctx, artist::rect user_bounds, draw_fn render)
   {
      ctx.resume();
      auto mode = current().composite;
      auto main = ctx.target();

      // Fast path: plain source-over draws straight to the target.
      float const blur = current().blur;
      device_context* dc = nullptr;
      if ((mode == canvas::source_over && blur == 0) ||
          FAILED(main->QueryInterface(&dc)) || !dc)
      {
         main->SetTransform(current().matrix);
         render(main);
         release(dc);
         return;
      }

      // Render the source primitive into a fresh, input-capable target bitmap via
      // a *separate* device context (the main one is mid-BeginDraw, so it can't be
      // retargeted). A D2D1_BITMAP_OPTIONS_TARGET bitmap can be both rendered to
      // and used as a DrawImage source / blend-effect input, unlike the bitmap
      // behind a CreateCompatibleRenderTarget, which is not a valid effect input.
      auto sz = main->GetPixelSize();

      // Every intermediate has to live in the target's coordinate space. A
      // Direct2D bitmap carries its own DPI and DrawImage scales it by that,
      // so bitmaps left at the default 96 DPI are drawn enlarged onto a scaled
      // target (an offscreen image renders at size * scale). Tag them with the
      // target's DPI instead, so pixels map to DIPs exactly as they do there.
      FLOAT dpi_x = 96.0f, dpi_y = 96.0f;
      main->GetDpi(&dpi_x, &dpi_y);

      ID2D1Device* device = nullptr;
      device_context* sdc = nullptr;
      ID2D1Bitmap1* src_bm = nullptr;
      dc->GetDevice(&device);
      bool ready =
         device &&
         SUCCEEDED(device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE, &sdc)) && sdc &&
         SUCCEEDED(sdc->CreateBitmap(
            sz, nullptr, 0,
            D2D1::BitmapProperties1(
               D2D1_BITMAP_OPTIONS_TARGET,
               D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
               dpi_x, dpi_y),
            &src_bm)) && src_bm;

      if (!ready)
      {
         // Couldn't build the intermediate; draw plainly rather than drop it.
         main->SetTransform(current().matrix);
         render(main);
         release(src_bm); release(sdc); release(device); release(dc);
         return;
      }

      sdc->SetTarget(src_bm);
      sdc->SetDpi(dpi_x, dpi_y);
      sdc->BeginDraw();
      sdc->Clear(D2D1::ColorF(0, 0, 0, 0));
      sdc->SetTransform(current().matrix);
      render(sdc);
      sdc->EndDraw();
      sdc->SetTarget(nullptr);   // unbind so src_bm can be used as an input

      // The source, blurred if there is a blur. The radius is the Gaussian
      // standard deviation, in the canvas's initial units.
      ID2D1Image* src_img = src_bm;          // non-owning alias
      effect* gauss = nullptr;
      ID2D1Image* gauss_out = nullptr;       // owned
      if (blur > 0 &&
          SUCCEEDED(dc->CreateEffect(CLSID_D2D1GaussianBlur, &gauss)) && gauss)
      {
         gauss->SetInput(0, src_bm);
         gauss->SetValue(D2D1_GAUSSIANBLUR_PROP_STANDARD_DEVIATION, blur);
         gauss->SetValue(
            D2D1_GAUSSIANBLUR_PROP_BORDER_MODE, D2D1_BORDER_MODE_SOFT);
         gauss->GetOutput(&gauss_out);
         if (gauss_out)
            src_img = gauss_out;
      }

      // Decide what to draw and how, BEFORE installing the clip. The destination
      // snapshot (CopyFromRenderTarget) must NOT happen inside an axis-aligned
      // clip: it returns failure there, which silently drops the blend.
      ID2D1Image* draw_img = nullptr;        // what to composite (non-owning alias)
      D2D1_COMPOSITE_MODE draw_mode = D2D1_COMPOSITE_MODE_SOURCE_OVER;
      bitmap* bg = nullptr;                  // background snapshot (blend only)
      effect* blend = nullptr;               // blend effect (blend only)
      ID2D1Image* blend_out = nullptr;       // blend effect output (owned)

      if (composite_is_blend(mode))
      {
         // Blend modes need both operands. Snapshot the live target as the
         // background (a plain bitmap is a valid CopyFromRenderTarget dest and
         // blend input), then blend the source over it and source-copy the result.
         auto props = D2D1::BitmapProperties(
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
            dpi_x, dpi_y);
         D2D1_POINT_2U dp{0, 0};
         D2D1_RECT_U sr{0, 0, sz.width, sz.height};
         main->Flush(nullptr, nullptr);
         suspend_clips();
         bool const copied =
            SUCCEEDED(main->CreateBitmap(sz, nullptr, 0, &props, &bg)) && bg &&
            SUCCEEDED(bg->CopyFromRenderTarget(&dp, main, &sr));
         resume_clips();
         if (copied && SUCCEEDED(dc->CreateEffect(CLSID_D2D1Blend, &blend)) && blend)
         {
            blend->SetInput(0, bg);       // background (destination)
            blend->SetInput(1, src_img);  // foreground (source)
            blend->SetValue(D2D1_BLEND_PROP_MODE, to_blend_mode(mode));
            blend->GetOutput(&blend_out); // an effect is not an ID2D1Image; its output is
            draw_img = blend_out;
            draw_mode = D2D1_COMPOSITE_MODE_SOURCE_COPY;
         }
      }
      else
      {
         // Porter-Duff: DrawImage composites the source onto the live target
         // (the destination) directly, in the requested mode.
         draw_img = src_img;
         draw_mode = to_composite_mode(mode);
      }

      // The unbounded operators clear the destination outside what is drawn,
      // as far as the clip, so they composite the whole intermediate. The
      // others are confined to the primitive's device-space bounds.
      if (draw_img)
      {
         bool const confine = !is_unbounded(mode);
         dc->SetTransform(D2D1::Matrix3x2F::Identity());
         if (confine)
         {
            // A Gaussian has all but faded three deviations out.
            auto db = device_bounds(current().matrix, user_bounds);
            db = db.inset(-3 * blur, -3 * blur);
            dc->PushAxisAlignedClip(
               D2D1::RectF(db.left, db.top, db.right, db.bottom),
               D2D1_ANTIALIAS_MODE_ALIASED);
         }
         dc->DrawImage(draw_img, D2D1_INTERPOLATION_MODE_LINEAR, draw_mode);
         if (confine)
            dc->PopAxisAlignedClip();
      }

      release(blend_out);
      release(blend);
      release(bg);
      release(gauss_out);
      release(gauss);
      release(src_bm);
      release(sdc);
      release(device);
      release(dc);
   }

   void canvas::canvas_state::fill(context& ctx, bool preserve)
   {
      if (shadow_visible() && !shape_shadow(ctx))
      {
         auto render =
            [this](render_target* target, brush* b, bool preserve)
            { _path.impl()->fill(*target, b, preserve); };
         auto bounds = _path.impl()->fill_bounds();
         adjust_for_blur(bounds);
         // Leaves the transform at identity.
         apply_blur(ctx, bounds, render, false);
      }
      // Computing the bounds realizes the geometry, which for a plain shape
      // costs more than drawing it.
      artist::rect bounds;
      if (needs_bounds())
         bounds = _path.impl()->fill_bounds();
      composite_draw(ctx, bounds,
         [this, preserve](render_target* t)
         { _path.impl()->fill(*t, fill_paint(*t), preserve); });
   }

   void canvas::canvas_state::stroke(context& ctx, bool preserve)
   {
      auto lw = current().line_width;
      auto ss = stroke_style_obj();
      if (shadow_visible())
      {
         auto render =
            [this, lw, ss](render_target* target, brush* b, bool preserve)
            { _path.impl()->stroke(*target, b, lw, preserve, ss); };
         auto bounds = _path.impl()->stroke_bounds(lw, ss);
         adjust_for_blur(bounds);
         // Leaves the transform at identity.
         apply_blur(ctx, bounds, render, true);
      }
      artist::rect bounds;
      if (needs_bounds())
         bounds = _path.impl()->stroke_bounds(lw, ss);
      composite_draw(ctx, bounds,
         [this, lw, ss, preserve](render_target* t)
         { _path.impl()->stroke(*t, stroke_paint(*t), lw, preserve, ss); });
   }

   ////////////////////////////////////////////////////////////////////////////
   // canvas
   ////////////////////////////////////////////////////////////////////////////
   canvas::canvas(canvas_impl* context_)
    : _context{context_}
    , _state{std::make_unique<canvas_state>()}
   {
      _context->state(_state.get());
      _state->set_target(_context->target());

      // The transform belongs to the context, as on the other backends: a
      // canvas starts from what the last one left, and leaves its own behind.
      // The hosts reset it at the start of each frame.
      if (auto t = _context->target())
         t->GetTransform(&_state->current().matrix);
   }

   canvas::~canvas()
   {
      if (auto t = _context->target())
         t->SetTransform(_state->current().matrix);
   }

   ////////////////////////////////////////////////////////////////////////////
   // Transforms
   void canvas::translate(point p)
   {
      auto& m = _state->current().matrix;
      m = matrix2x2f::Translation({p.x, p.y}) * m;
   }

   void canvas::rotate(float rad)
   {
      auto& m = _state->current().matrix;
      m = matrix2x2f::Rotation(rad * 180 / pi, {0, 0}) * m;
   }

   void canvas::scale(point p)
   {
      auto& m = _state->current().matrix;
      m = matrix2x2f::Scale({p.x, p.y}, {0, 0}) * m;
   }

   void canvas::skew(double sx, double sy)
   {
      auto& m = _state->current().matrix;
      // sx and sy are angles, as for affine_transform::skew: sx shears y with
      // x, sy shears x with y. Direct2D's Skew takes degrees, angleX first,
      // and that one is the shear of x with y.
      auto sk = matrix2x2f::Skew(
         float(sy * 180 / pi), float(sx * 180 / pi), {0, 0});
      m = sk * m;
   }

   point canvas::device_to_user(point p)
   {
      auto m = _state->current().matrix;
      m.Invert();
      auto q = m.TransformPoint({p.x, p.y});
      return {q.x, q.y};
   }

   point canvas::user_to_device(point p)
   {
      auto q = _state->current().matrix.TransformPoint({p.x, p.y});
      return {q.x, q.y};
   }

   affine_transform canvas::transform() const
   {
      auto const& m = _state->current().matrix;
      return affine_transform{m._11, m._12, m._21, m._22, m._31, m._32};
   }

   void canvas::transform(affine_transform const& mat)
   {
      auto& m = _state->current().matrix;
      m = matrix2x2f(
         float(mat.a), float(mat.b), float(mat.c),
         float(mat.d), float(mat.tx), float(mat.ty));
   }

   void canvas::transform(double a, double b, double c, double d, double tx, double ty)
   {
      transform(affine_transform{a, b, c, d, tx, ty});
   }

   ////////////////////////////////////////////////////////////////////////////
   // State
   void canvas::save()
   {
      _state->save();
   }

   void canvas::restore()
   {
      _state->restore();
   }

   void canvas::begin_group(float opacity)
   {
      _state->save();
      _state->do_group(opacity);
   }

   void canvas::end_group()
   {
      _state->restore();
   }

   ////////////////////////////////////////////////////////////////////////////
   // Paths
   // begin_path empties the current path, as beginPath does in the W3C API.
   // path_impl::begin_path only closes the open sub-path.
   void canvas::begin_path()
   {
      auto* p = _state->path().impl();
      p->clear();
      p->begin_path();
   }

   void canvas::close_path()
   {
      _state->path().impl()->close_path();
   }

   // The fill rule is drawing state, not a property of the path being built,
   // so it is stamped onto the current path at the point of use. path_impl
   // keeps its cached geometry when the mode is unchanged, which it usually
   // is, so this costs nothing in the common case.
   void canvas::fill()
   {
      _state->path().fill_rule(_state->current().fill_rule);
      _state->fill(*_context, false);
   }

   void canvas::fill_preserve()
   {
      _state->path().fill_rule(_state->current().fill_rule);
      _state->fill(*_context, true);
   }

   void canvas::stroke()
   {
      _state->stroke(*_context, false);
   }

   void canvas::stroke_preserve()
   {
      _state->stroke(*_context, true);
   }

   void canvas::clip()
   {
      _context->resume();
      // A single rectangle under a transform without rotation or skew clips
      // axis-aligned: no geometry, no layer.
      _state->path().fill_rule(_state->current().fill_rule);
      artist::rect r;
      auto const& m = _state->current().matrix;
      if (m._12 == 0 && m._21 == 0 && _state->path().impl()->rect_primitive(r))
      {
         _state->do_clip(r);
         _state->path().impl()->clear();
         return;
      }

      bool owned = false;
      auto geo = _state->path().impl()->realize_fill(owned);
      // clip() consumes the current path (matches the other backends' detach);
      // otherwise the clip geometry lingers and a following fill_rect/fill would
      // also paint it.
      if (geo)
      {
         geo->AddRef();   // kept alive until the layer is popped
         _state->do_clip(geo);
      }
      _state->path().impl()->clear();
   }

   void canvas::clip(class path const& p)
   {
      _context->resume();
      // Copy so we can realize the geometry (build_path mutates); the copy's
      // path_impl owns the cached geometry, so AddRef to outlive it.
      path tmp{p};
      bool owned = false;
      auto geo = tmp.impl()->realize_fill(owned);
      if (!geo)
         return;
      geo->AddRef();
      _state->do_clip(geo);
   }

   rect canvas::clip_extent() const
   {
      // The device-space clip, kept by canvas_state as clips push and pop,
      // brought back to user space as its bounding box under the inverse of
      // the current transform. Direct2D has no query for the active clip.
      auto t = _context->target();
      if (!t)
         return {0, 0, 0, 0};
      auto s = t->GetSize();   // DIPs
      artist::rect device{0, 0, s.width, s.height};
      if (auto c = _state->device_clip())
         device = artist::intersection(device, *c);
      auto inv = _state->current().matrix;
      if (!inv.Invert())
         return device;
      return device_bounds(inv, device);
   }

   bool canvas::point_in_path(point p) const
   {
      _state->path().fill_rule(_state->current().fill_rule);
      return _state->path().includes(p);
   }

   rect canvas::fill_extent() const
   {
      return _state->path().impl()->fill_bounds();
   }

   void canvas::move_to(point p)
   {
      _state->path().impl()->move_to(p);
   }

   void canvas::line_to(point p)
   {
      _state->path().impl()->line_to(p);
   }

   void canvas::arc_to(point p1, point p2, float radius)
   {
      _state->path().impl()->arc_to(p1, p2, radius);
   }

   void canvas::arc(
      point p, float radius,
      float start_angle, float end_angle,
      bool ccw)
   {
      _state->path().arc(p, radius, start_angle, end_angle, ccw);
   }

   void canvas::add_rect(rect const& r)
   {
      _state->path().add_rect(r);
   }

   canvas::path_holder::path_holder(canvas& cnv)
    : _cnv(cnv)
    , _saved(new d2d::path_impl(*cnv._state->path().impl()))
   {
      cnv.begin_path();
   }

   canvas::path_holder::~path_holder()
   {
      auto* saved = static_cast<d2d::path_impl*>(_saved);
      *_cnv._state->path().impl() = *saved;
      delete saved;
   }

   void canvas::add_round_rect_impl(rect const& r, float radius)
   {
      _state->path().add_round_rect(r, radius);
   }

   void canvas::add_path(path const& p)
   {
      if (!p.impl())
         return;
      // Copy the source path's generators (self-contained), fold any pending
      // sub-path, and append to the current path. The current transform is
      // applied at fill/stroke time, so the path's user-space coords compose
      // with the canvas CTM (matches the other backends).
      d2d::path_impl src{*p.impl()};
      src.flatten();
      _state->path().impl()->absorb(src);
   }

   void canvas::clear_rect(rect const& r)
   {
      _context->resume();
      auto t = _context->target();
      if (!t)
         return;
      t->SetTransform(_state->current().matrix);
      t->PushAxisAlignedClip(
         D2D1::RectF(r.left, r.top, r.right, r.bottom),
         D2D1_ANTIALIAS_MODE_ALIASED);
      t->Clear(D2D1::ColorF(0, 0, 0, 0));
      t->PopAxisAlignedClip();
   }

   void canvas::quadratic_curve_to(point cp, point end)
   {
      _state->path().impl()->quadratic_curve_to(cp, end);
   }

   void canvas::bezier_curve_to(point cp1, point cp2, point end)
   {
      _state->path().impl()->bezier_curve_to(cp1, cp2, end);
   }

   ////////////////////////////////////////////////////////////////////////////
   // Styles
   void canvas::fill_style(color c)            { _state->set_fill(c); }
   void canvas::stroke_style(color c)          { _state->set_stroke(c); }
   void canvas::fill_style(linear_gradient const& gr)   { _state->set_fill(gr); }
   void canvas::fill_style(radial_gradient const& gr)   { _state->set_fill(gr); }
   void canvas::stroke_style(linear_gradient const& gr) { _state->set_stroke(gr); }
   void canvas::stroke_style(radial_gradient const& gr) { _state->set_stroke(gr); }

   void canvas::line_width(float w)
   {
      // Zero, negative, infinite and NaN widths are ignored.
      if (!(w > 0) || !std::isfinite(w))
         return;
      _state->current().line_width = w;
   }

   void canvas::line_cap(line_cap_enum cap)
   {
      _state->current().line_cap = cap;
      _state->discard();   // stroke_style rebuilds lazily
   }

   void canvas::line_join(join_enum join)
   {
      _state->current().join = join;
      _state->discard();
   }

   void canvas::miter_limit(float limit)
   {
      // Zero, negative, infinite and NaN limits are ignored.
      if (!(limit > 0) || !std::isfinite(limit))
         return;
      _state->current().miter_limit = limit;
      _state->discard();
   }

   void canvas::shadow_style(point offset, float blur, color c)
   {
      auto& cur = _state->current();
      cur.shadow_offset = offset;
      cur.shadow_blur = blur;
      cur.shadow_color = c;
   }

   void canvas::blur_style(float radius)
   {
      // Negative, infinite and NaN radii are ignored; zero is no blur.
      if (!(radius >= 0) || !std::isfinite(radius))
         return;
      _state->current().blur = radius;
   }

   void canvas::global_composite_operation(composite_op_enum mode)
   {
      _state->current().composite = mode;
   }

   void canvas::fill_rule(path::fill_rule_enum rule)
   {
      _state->current().fill_rule = rule;
   }

   ////////////////////////////////////////////////////////////////////////////
   // Text (DirectWrite)
   void canvas::font(class font const& font_)
   {
      if (font_)
         _state->current().font = font_;
   }

   namespace
   {
      // Aligned left-edge x and baseline y for an anchor point `p`, given the
      // text width and font ascent/descent. Mirrors the Quartz alignment.
      void align_text(int align, point& left_baseline, float width, float a, float d)
      {
         switch (align & 0x3)   // horizontal
         {
            case canvas::center: left_baseline.x -= width / 2; break;
            case canvas::right:  left_baseline.x -= width;     break;
            default: break;
         }
         switch (align & 0x1C)  // vertical (baseline-origin)
         {
            case canvas::top:    left_baseline.y += a;             break;
            case canvas::middle: left_baseline.y += (a - d) / 2;   break;
            case canvas::bottom: left_baseline.y -= d;             break;
            default: break;     // baseline
         }
      }
   }

   void canvas::fill_text(std::string_view utf8, point p)
   {
      auto fi = _state->current().font.impl();
      auto t = _context->target();
      if (!fi || !fi->format || !t)
         return;

      auto wtext = d2d::to_utf16(utf8);
      IDWriteTextLayout* layout = nullptr;
      if (FAILED(d2d::dwrite_factory()->CreateTextLayout(
            wtext.c_str(), UINT32(wtext.size()), fi->format,
            FLT_MAX, FLT_MAX, &layout)) || !layout)
         return;

      DWRITE_TEXT_METRICS tm{};
      layout->GetMetrics(&tm);

      point lb = p;
      align_text(_state->current().text_align, lb, tm.widthIncludingTrailingWhitespace,
         fi->ascent, fi->descent);

      D2D1_POINT_2F org = D2D1::Point2F(lb.x, lb.y - fi->ascent);   // layout box top-left

      // Drop shadow / glow: render the text in the shadow color into an offscreen,
      // blur + offset it, and composite under the real text.
      if (_state->shadow_visible())
         _state->apply_blur(*_context, {},
            [layout, org](render_target* target, brush* b, bool)
            {
               target->DrawTextLayout(org, layout, b, D2D1_DRAW_TEXT_OPTIONS_NONE);
            }, false);

      auto paint = [&](render_target* target)
      {
         auto brush = _state->fill_paint(*target);
         target->DrawTextLayout(org, layout, brush, D2D1_DRAW_TEXT_OPTIONS_NONE);
      };
      if (_state->needs_bounds())
      {
         // The layout box, grown by a quarter of the font size for glyphs
         // that overhang it, such as italics.
         auto pad = fi->size / 4;
         artist::rect bounds{
            org.x - pad, org.y - pad
          , org.x + tm.widthIncludingTrailingWhitespace + pad
          , org.y + tm.height + pad};
         _state->composite_draw(*_context, bounds, paint);
      }
      else
      {
         t->SetTransform(_state->current().matrix);
         paint(t);
      }
      d2d::release(layout);
   }

   void canvas::stroke_text(std::string_view utf8, point p)
   {
      auto fi = _state->current().font.impl();
      auto t = _context->target();
      if (!fi || !fi->face || !t)
         return;

      auto cps = to_utf32(utf8);
      if (cps.empty())
         return;
      std::vector<UINT32> codes(cps.begin(), cps.end());
      std::vector<UINT16> glyphs(codes.size());
      fi->face->GetGlyphIndices(codes.data(), UINT32(codes.size()), glyphs.data());

      std::vector<DWRITE_GLYPH_METRICS> gm(glyphs.size());
      fi->face->GetDesignGlyphMetrics(glyphs.data(), UINT32(glyphs.size()), gm.data(), FALSE);
      DWRITE_FONT_METRICS fmx{};
      fi->face->GetMetrics(&fmx);
      float scale = fi->size / float(fmx.designUnitsPerEm);

      std::vector<FLOAT> advances(glyphs.size());
      float width = 0;
      for (size_t i = 0; i != glyphs.size(); ++i)
      {
         advances[i] = gm[i].advanceWidth * scale;
         width += advances[i];
      }

      // Glyph outline is emitted at baseline origin (0,0), y-down.
      ID2D1PathGeometry* geo = nullptr;
      if (FAILED(d2d::get_factory().CreatePathGeometry(&geo)) || !geo)
         return;
      ID2D1GeometrySink* sink = nullptr;
      geo->Open(&sink);
      fi->face->GetGlyphRunOutline(
         fi->size, glyphs.data(), advances.data(), nullptr,
         UINT32(glyphs.size()), FALSE, FALSE, sink
      );
      sink->Close();
      d2d::release(sink);

      point lb = p;
      align_text(_state->current().text_align, lb, width, fi->ascent, fi->descent);

      auto lw = _state->current().line_width;
      auto ss = _state->stroke_style_obj();

      // Move the outline to the baseline origin as a transformed geometry
      // rather than folding the translation into the target transform, so a
      // gradient stroke brush keeps its user-space coordinates.
      ID2D1TransformedGeometry* placed = nullptr;
      d2d::get_factory().CreateTransformedGeometry(
         geo, D2D1::Matrix3x2F::Translation(lb.x, lb.y), &placed);
      d2d::release(geo);
      if (!placed)
         return;

      // Drop shadow / glow for the stroked outline.
      if (_state->shadow_visible())
         _state->apply_blur(*_context, {},
            [placed, lw, ss](render_target* target, brush* b, bool)
            {
               target->DrawGeometry(placed, b, lw, ss);
            }, true);

      auto paint = [&](render_target* target)
      {
         target->DrawGeometry(placed, _state->stroke_paint(*target), lw, ss);
      };
      if (_state->needs_bounds())
      {
         D2D1_RECT_F b{};
         placed->GetWidenedBounds(lw, ss, nullptr, &b);
         _state->composite_draw(*_context, {b.left, b.top, b.right, b.bottom}, paint);
      }
      else
      {
         t->SetTransform(_state->current().matrix);
         paint(t);
      }
      d2d::release(placed);
   }

   canvas::text_metrics canvas::measure_text(std::string_view utf8)
   {
      auto const& f = _state->current().font;
      auto fi = f.impl();
      if (!fi || !fi->format)
         return {};

      auto measure = [&]() -> text_metrics
      {
         auto wtext = d2d::to_utf16(utf8);
         IDWriteTextLayout* layout = nullptr;
         float width = 0;
         if (SUCCEEDED(d2d::dwrite_factory()->CreateTextLayout(
               wtext.c_str(), UINT32(wtext.size()), fi->format,
               FLT_MAX, FLT_MAX, &layout)) && layout)
         {
            DWRITE_TEXT_METRICS tm{};
            layout->GetMetrics(&tm);
            width = tm.widthIncludingTrailingWhitespace;
            d2d::release(layout);
         }
         return text_metrics{
            fi->ascent, fi->descent, fi->leading,
            {width, fi->ascent + fi->descent + fi->leading}
         };
      };

      return artist::detail::get_measure_cache<artist::font, text_metrics>().get(
         fi->format, f, utf8, measure);
   }

   void canvas::text_align(int align)
   {
      _state->current().text_align = align;
   }

   void canvas::text_align(text_halign align)
   {
      auto& a = _state->current().text_align;
      a = (a & ~0x3) | align;
   }

   void canvas::text_baseline(text_valign align)
   {
      auto& a = _state->current().text_align;
      a = (a & ~0x1C) | align;
   }

   ////////////////////////////////////////////////////////////////////////////
   // Pixmaps
   void canvas::draw(image const& pic, rect const& src, rect const& dest)
   {
      if (!_context->target())
         return;

      // src is in the image's own units; the bitmap source rectangle is in
      // pixels, so a scaled image needs it scaled too.
      auto sc = pic.scale();
      auto paint = [&](render_target* t)
      {
         // The device bitmap is cached on the image (see image_bitmap), so
         // a photo drawn every frame is uploaded once, not every frame.
         auto bm = d2d::image_bitmap(pic, *t);
         if (!bm)
            return;
         t->DrawBitmap(
            bm,
            D2D1::RectF(dest.left, dest.top, dest.right, dest.bottom),
            1.0f,
            D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
            D2D1::RectF(
               src.left * sc, src.top * sc, src.right * sc, src.bottom * sc)
         );
      };

      // Images cast shadows too, from their own alpha.
      if (_state->shadow_visible())
         _state->apply_blur(*_context, {},
            [&](render_target* t, brush*, bool) { paint(t); }, false);

      _state->composite_draw(*_context, dest, paint);
   }

   void canvas::draw(canvas_layer const& layer, rect const& dest)
   {
      auto* impl = layer.impl();
      if (!_context->target() || !impl || !impl->rt)
         return;

      // The layer's bitmap can be taken only outside its BeginDraw.
      if (impl->drawing)
      {
         impl->rt->EndDraw();
         impl->drawing = false;
      }

      d2d::bitmap* bm = nullptr;
      impl->rt->GetBitmap(&bm);
      if (bm)
      {
         auto paint = [&](render_target* t)
         {
            t->DrawBitmap(
               bm,
               D2D1::RectF(dest.left, dest.top, dest.right, dest.bottom),
               1.0f,
               D2D1_BITMAP_INTERPOLATION_MODE_LINEAR
            );
         };

         if (_state->shadow_visible())
            _state->apply_blur(*_context, {},
               [&](render_target* t, brush*, bool) { paint(t); }, false);

         _state->composite_draw(*_context, dest, paint);
         d2d::release(bm);
      }

      impl->rt->BeginDraw();
      impl->drawing = true;
   }
}
