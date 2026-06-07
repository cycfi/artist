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
#include "context.hpp"
#include "path_impl.hpp"
#include "font_impl.hpp"
#include <variant>
#include <stack>
#include <vector>
#include <cmath>

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
         class font     font           = font_descr{"Segoe UI", 12};
         int            text_align     = canvas::baseline;
         composite_op_enum composite   = canvas::source_over;
         std::size_t    clip_depth     = 0;   // # of clip layers active at save
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
      // current matrix) honoring the current global composite op. `user_bounds`
      // is the primitive's bounding rect in user space (the affected region).
      using draw_fn = std::function<void(render_target*)>;
      void              composite_draw(context& ctx, artist::rect user_bounds, draw_fn render);

      // Drop-shadow blur. Public so canvas::fill_text / stroke_text can apply the
      // current shadow under text the same way fill()/stroke() do under paths:
      // `render` draws the primitive (in the supplied shadow brush) into an
      // offscreen, which is Gaussian-blurred, offset, and composited onto the
      // main target. Leaves the main transform at identity.
      using render_function = std::function<void(render_target*, brush*, bool)>;
      void              apply_blur(context& ctx, artist::rect bounds, render_function render);
      void              adjust_for_blur(artist::rect& bounds);

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
                           ID2D1Layer* layer = nullptr;
                           if (FAILED(_rt->CreateLayer(nullptr, &layer)) || !layer)
                           {
                              release(geo);
                              return;
                           }
                           _rt->SetTransform(current().matrix);
                           _rt->PushLayer(
                              D2D1::LayerParameters(
                                 D2D1::InfiniteRect(), geo,
                                 D2D1_ANTIALIAS_MODE_PER_PRIMITIVE,
                                 D2D1::IdentityMatrix(), 1.0f, nullptr,
                                 D2D1_LAYER_OPTIONS_NONE),
                              layer);
                           _clips.push_back({layer, geo});
                        }
      std::size_t       clip_count() const             { return _clips.size(); }
      void              pop_clips_to(std::size_t depth)
                        {
                           while (_clips.size() > depth)
                           {
                              auto c = _clips.back();
                              _clips.pop_back();
                              if (_rt)
                                 _rt->PopLayer();
                              release(c.layer);
                              release(c.geo);
                           }
                        }

   private:

      struct clip_t { ID2D1Layer* layer; geometry* geo; };

      std::stack<info>  _stack;
      artist::path      _path;
      render_target*    _rt = nullptr;
      std::vector<clip_t> _clips;

      brush*            _fill_paint = nullptr;     // lazy, derived from fill_info
      brush*            _stroke_paint = nullptr;   // lazy, derived from stroke_info
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
      release(_stroke_style);
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
      release(_stroke_style);
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
         // Derived resources depend on the (now restored) info; rebuild lazily.
         release(_fill_paint);
         release(_stroke_paint);
         release(_stroke_style);
      }
   }

   void canvas::canvas_state::set_fill(paint_info const& info)
   {
      current().fill_info = info;
      release(_fill_paint);
   }

   void canvas::canvas_state::set_stroke(paint_info const& info)
   {
      current().stroke_info = info;
      release(_stroke_paint);
   }

   brush* canvas::canvas_state::fill_paint(render_target& target)
   {
      if (!_fill_paint)
         _fill_paint = std::visit(
            [&](auto const& i){ return make_paint(i, target); }, current().fill_info);
      return _fill_paint;
   }

   brush* canvas::canvas_state::stroke_paint(render_target& target)
   {
      if (!_stroke_paint)
         _stroke_paint = std::visit(
            [&](auto const& i){ return make_paint(i, target); }, current().stroke_info);
      return _stroke_paint;
   }

   d2d::stroke_style* canvas::canvas_state::stroke_style_obj()
   {
      if (!_stroke_style)
         _stroke_style = make_stroke_style(
            current().line_cap, current().join, current().miter_limit);
      return _stroke_style;
   }

   void canvas::canvas_state::adjust_for_blur(artist::rect& bounds)
   {
      float offset = current().shadow_blur / current().matrix.m11;
      bounds.left -= offset;
      bounds.right += offset;
      bounds.top -= offset;
      bounds.bottom += offset;
   }

   void canvas::canvas_state::apply_blur(
      context& ctx, artist::rect /*bounds*/, render_function render)
   {
      auto& cur = current();

      // The blurred shadow must be composited onto the MAIN target, so we need
      // the main target as an ID2D1DeviceContext (D2D 1.1). If unavailable,
      // skip the shadow rather than mis-render.
      device_context* dc = nullptr;
      if (FAILED(ctx.target()->QueryInterface(&dc)) || !dc)
         return;

      float alpha = 1.0f;
      if (std::holds_alternative<color>(cur.fill_info))
         alpha = std::get<color>(cur.fill_info).alpha;

      // Render the shape (in shadow color) into a compatible offscreen bitmap,
      // positioned exactly like the real shape (same transform).
      offscreen_context offscreen{ctx};
      auto bm_target = offscreen.target();
      bm_target->BeginDraw();
      bm_target->Clear(D2D1::ColorF(0, 0, 0, 0));
      bm_target->SetTransform(cur.matrix);
      solid_color_brush* shadow_paint = nullptr;
      bm_target->CreateSolidColorBrush(
         D2D1::ColorF(
            cur.shadow_color.red, cur.shadow_color.green,
            cur.shadow_color.blue, alpha),
         &shadow_paint
      );
      render(bm_target, shadow_paint, true);
      bm_target->EndDraw();

      // Gaussian-blur the shadow bitmap, offset it, and draw it onto the main
      // target (under the shape, which the caller paints next).
      effect* blur = nullptr;
      effect* xform = nullptr;
      if (SUCCEEDED(dc->CreateEffect(CLSID_D2D1GaussianBlur, &blur)) && blur &&
          SUCCEEDED(dc->CreateEffect(CLSID_D2D12DAffineTransform, &xform)) && xform)
      {
         // Match the Skia backend: shadow blur maps directly to the Gaussian
         // standard deviation (Skia uses sigma = blur). The earlier /2 made the
         // D2D shadow about half as wide (too tight/hard).
         float blur_val = cur.shadow_blur / cur.matrix.m11;
         blur->SetInput(0, offscreen.bitmap());
         blur->SetValue(D2D1_GAUSSIANBLUR_PROP_BORDER_MODE, D2D1_BORDER_MODE_SOFT);
         blur->SetValue(D2D1_GAUSSIANBLUR_PROP_STANDARD_DEVIATION, blur_val);
         blur->SetValue(D2D1_GAUSSIANBLUR_PROP_OPTIMIZATION, D2D1_GAUSSIANBLUR_OPTIMIZATION_BALANCED);

         float offset_x = cur.shadow_offset.x;
         float offset_y = cur.shadow_offset.y;
         xform->SetInputEffect(0, blur);
         xform->SetValue(
            D2D1_2DAFFINETRANSFORM_PROP_TRANSFORM_MATRIX,
            D2D1::Matrix3x2F::Translation(offset_x, offset_y));

         dc->SetTransform(D2D1::Matrix3x2F::Identity());
         dc->DrawImage(xform, D2D1_INTERPOLATION_MODE_LINEAR);
      }

      release(blur);
      release(xform);
      release(shadow_paint);
      release(dc);
   }

   void canvas::canvas_state::composite_draw(
      context& ctx, artist::rect user_bounds, draw_fn render)
   {
      auto mode = current().composite;
      auto main = ctx.target();

      // Fast path: plain source-over draws straight to the target.
      device_context* dc = nullptr;
      if (mode == canvas::source_over ||
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
      // and used as a DrawImage source / blend-effect input — unlike the bitmap
      // behind a CreateCompatibleRenderTarget, which is not a valid effect input.
      auto sz = main->GetPixelSize();
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
               D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)),
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
      sdc->BeginDraw();
      sdc->Clear(D2D1::ColorF(0, 0, 0, 0));
      sdc->SetTransform(current().matrix);
      render(sdc);
      sdc->EndDraw();
      sdc->SetTarget(nullptr);   // unbind so src_bm can be used as an input

      // Decide what to draw and how, BEFORE installing the clip. The destination
      // snapshot (CopyFromRenderTarget) must NOT happen inside an axis-aligned
      // clip — it returns failure there, which silently drops the blend.
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
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
         D2D1_POINT_2U dp{0, 0};
         D2D1_RECT_U sr{0, 0, sz.width, sz.height};
         main->Flush(nullptr, nullptr);
         if (SUCCEEDED(main->CreateBitmap(sz, nullptr, 0, &props, &bg)) && bg &&
             SUCCEEDED(bg->CopyFromRenderTarget(&dp, main, &sr)) &&
             SUCCEEDED(dc->CreateEffect(CLSID_D2D1Blend, &blend)) && blend)
         {
            blend->SetInput(0, bg);       // background (destination)
            blend->SetInput(1, src_bm);   // foreground (source)
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
         draw_img = src_bm;
         draw_mode = to_composite_mode(mode);
      }

      // The composite is confined to the primitive's device-space bounds — canvas
      // ops affect only the draw's coverage, not the whole surface (matches Skia).
      if (draw_img)
      {
         auto db = device_bounds(current().matrix, user_bounds);
         dc->SetTransform(D2D1::Matrix3x2F::Identity());
         dc->PushAxisAlignedClip(
            D2D1::RectF(db.left, db.top, db.right, db.bottom),
            D2D1_ANTIALIAS_MODE_ALIASED);
         dc->DrawImage(draw_img, D2D1_INTERPOLATION_MODE_LINEAR, draw_mode);
         dc->PopAxisAlignedClip();
      }

      release(blend_out);
      release(blend);
      release(bg);
      release(src_bm);
      release(sdc);
      release(device);
      release(dc);
   }

   void canvas::canvas_state::fill(context& ctx, bool preserve)
   {
      if (current().shadow_blur != 0)
      {
         auto render =
            [this](render_target* target, brush* b, bool preserve)
            { _path.impl()->fill(*target, b, preserve); };
         auto bounds = _path.impl()->fill_bounds();
         adjust_for_blur(bounds);
         apply_blur(ctx, bounds, render);   // leaves the transform at identity
      }
      auto bounds = _path.impl()->fill_bounds();
      composite_draw(ctx, bounds,
         [this, preserve](render_target* t)
         { _path.impl()->fill(*t, fill_paint(*t), preserve); });
   }

   void canvas::canvas_state::stroke(context& ctx, bool preserve)
   {
      auto lw = current().line_width;
      auto ss = stroke_style_obj();
      if (current().shadow_blur != 0)
      {
         auto render =
            [this, lw, ss](render_target* target, brush* b, bool preserve)
            { _path.impl()->stroke(*target, b, lw, preserve, ss); };
         auto bounds = _path.impl()->stroke_bounds(lw, ss);
         adjust_for_blur(bounds);
         apply_blur(ctx, bounds, render);   // leaves the transform at identity
      }
      auto bounds = _path.impl()->stroke_bounds(lw, ss);
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
   }

   canvas::~canvas()
   {
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
      auto sk = matrix2x2f::Skew(
         float(std::atan(sx) * 180 / pi), float(std::atan(sy) * 180 / pi), {0, 0});
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

   ////////////////////////////////////////////////////////////////////////////
   // Paths
   void canvas::begin_path()
   {
      _state->path().impl()->begin_path();
   }

   void canvas::close_path()
   {
      _state->path().impl()->close_path();
   }

   void canvas::fill()
   {
      _state->fill(*_context, false);
   }

   void canvas::fill_preserve()
   {
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
      if (auto t = _context->target())
      {
         auto s = t->GetSize();   // DIPs
         return {0, 0, s.width, s.height};
      }
      return {0, 0, 0, 0};
   }

   bool canvas::point_in_path(point p) const
   {
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

   void canvas::global_composite_operation(composite_op_enum mode)
   {
      _state->current().composite = mode;
   }

   void canvas::fill_rule(path::fill_rule_enum rule)
   {
      _state->path().fill_rule(rule);
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
      if (_state->current().shadow_blur != 0)
         _state->apply_blur(*_context, {},
            [layout, org](render_target* target, brush* b, bool)
            {
               target->DrawTextLayout(org, layout, b, D2D1_DRAW_TEXT_OPTIONS_NONE);
            });

      t->SetTransform(_state->current().matrix);
      auto brush = _state->fill_paint(*t);
      t->DrawTextLayout(org, layout, brush, D2D1_DRAW_TEXT_OPTIONS_NONE);
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
      auto xform = D2D1::Matrix3x2F::Translation(lb.x, lb.y) * _state->current().matrix;

      // Drop shadow / glow for the stroked outline (the glyph outline sits at the
      // origin, so the shadow render must re-apply the glyph translation).
      if (_state->current().shadow_blur != 0)
         _state->apply_blur(*_context, {},
            [geo, xform, lw, ss](render_target* target, brush* b, bool)
            {
               target->SetTransform(xform);
               target->DrawGeometry(geo, b, lw, ss);
            });

      t->SetTransform(xform);
      t->DrawGeometry(geo, _state->stroke_paint(*t), lw, ss);
      d2d::release(geo);
   }

   canvas::text_metrics canvas::measure_text(std::string_view utf8)
   {
      auto fi = _state->current().font.impl();
      if (!fi || !fi->format)
         return {};

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
   }

   void canvas::text_align(int align)
   {
      _state->current().text_align = align;
   }

   void canvas::text_align(text_halign align)
   {
      _state->current().text_align |= align;
   }

   void canvas::text_baseline(text_valign align)
   {
      _state->current().text_align |= align;
   }

   ////////////////////////////////////////////////////////////////////////////
   // Pixmaps
   void canvas::draw(image const& pic, rect const& src, rect const& dest)
   {
      auto wic = d2d::wic_bitmap(pic);
      if (!_context->target() || !wic)
         return;

      _state->composite_draw(*_context, dest,
         [&](render_target* t)
         {
            ID2D1Bitmap* bm = nullptr;
            if (FAILED(t->CreateBitmapFromWicBitmap(wic, &bm)) || !bm)
               return;
            t->DrawBitmap(
               bm,
               D2D1::RectF(dest.left, dest.top, dest.right, dest.bottom),
               1.0f,
               D2D1_BITMAP_INTERPOLATION_MODE_LINEAR,
               D2D1::RectF(src.left, src.top, src.right, src.bottom)
            );
            release(bm);
         });
   }
}
