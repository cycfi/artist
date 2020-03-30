/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <artist/canvas.hpp>
#include <stack>
#include "opaque.hpp"

#include <SkBitmap.h>
#include <SkData.h>
#include <SkImage.h>
#include <SkPicture.h>
#include <SkSurface.h>
#include <SkCanvas.h>
#include <SkPath.h>
#include <SkGradientShader.h>
#include <SkImageFilter.h>
#include <SkDropShadowImageFilter.h>
#include <SkTextBlob.h>
#include <SkTypeface.h>
#include <SkFont.h>

namespace cycfi::artist
{
   class canvas::canvas_state
   {
   public:

      struct blur_info
      {
         point    _offset;
         float    _blur;
         color    _color;
      };

      canvas_state();

      SkPath&           path();
      SkPaint&          fill_paint();
      SkPaint&          stroke_paint();
      class font&       font();
      int&              text_align();

      void              save();
      void              restore();

      point             _pre_scale = { 1.0, 1.0 };

      static SkPaint&   get_fill_paint(canvas const& cnv);

   private:

      struct state_info
      {
         state_info()
         {
            _fill_paint.setAntiAlias(true);
            _fill_paint.setStyle(SkPaint::kFill_Style);
            _stroke_paint.setAntiAlias(true);
            _stroke_paint.setStyle(SkPaint::kStroke_Style);
         }

         SkPath         _path;
         SkPaint        _fill_paint;
         SkPaint        _stroke_paint;
         class font     _font;
         int            _text_align = 0;
      };

      using state_info_ptr = std::unique_ptr<state_info>;
      using state_info_stack = std::stack<state_info_ptr>;

      state_info*       current() { return _stack.top().get(); }
      state_info const* current() const { return _stack.top().get(); }

      state_info_stack  _stack;
   };

   canvas::canvas_state::canvas_state()
   {
      _stack.push(std::make_unique<state_info>());
   }

   SkPath& canvas::canvas_state::path()
   {
      return current()->_path;
   }

   SkPaint& canvas::canvas_state::fill_paint()
   {
      return current()->_fill_paint;
   }

   SkPaint& canvas::canvas_state::stroke_paint()
   {
      return current()->_stroke_paint;
   }

   class font& canvas::canvas_state::font()
   {
      return current()->_font;
   }

   int& canvas::canvas_state::text_align()
   {
      return current()->_text_align;
   }

   void canvas::canvas_state::save()
   {
      _stack.push(std::make_unique<state_info>(*current()));
   }

   void canvas::canvas_state::restore()
   {
      if (_stack.size())
         _stack.pop();
   }

   SkPaint& canvas::canvas_state::get_fill_paint(canvas const& cnv)
   {
      return cnv._state->fill_paint();
   }

   SkPaint& fill_paint(canvas const& cnv)
   {
      return canvas::canvas_state::get_fill_paint(cnv);
   }

   canvas::canvas(canvas_impl_ptr context_)
    : _context{ context_ }
    , _state{ std::make_unique<canvas_state>() }
   {
   }

   canvas::~canvas()
   {
   }

   void canvas::pre_scale(point p)
   {
      scale(p);
      _state->_pre_scale = p;
   }

   void canvas::translate(point p)
   {
      _context->translate(p.x, p.y);
   }

   void canvas::rotate(float rad)
   {
      _context->rotate(rad * (180.0/M_PI));
   }

   void canvas::scale(point p)
   {
      _context->scale(p.x, p.y);
   }

   void canvas::save()
   {
      _context->save();
      _state->save();
   }

   void canvas::restore()
   {
      _context->restore();
      _state->restore();
   }

   void canvas::begin_path()
   {
      _state->path() = {};
   }

   void canvas::close_path()
   {
   }

   void canvas::fill()
   {
      fill_preserve();
      _state->path().reset();
   }

   void canvas::fill_preserve()
   {
      _context->drawPath(_state->path(), _state->fill_paint());
   }

   void canvas::stroke()
   {
      stroke_preserve();
      _state->path().reset();
   }

   void canvas::stroke_preserve()
   {
      _context->drawPath(_state->path(), _state->stroke_paint());
   }

   void canvas::clip()
   {
      _context->clipPath(_state->path(), true);
      _state->path().reset();
   }

   void canvas::move_to(point p)
   {
      _state->path().moveTo(p.x, p.y);
   }

   void canvas::line_to(point p)
   {
      _state->path().lineTo(p.x, p.y);
   }

   void canvas::arc_to(point p1, point p2, float radius)
   {
      _state->path().arcTo(p1.x, p1.y, p2.x, p2.y, radius);
   }

   void canvas::arc(
      point p, float radius,
      float start_angle, float end_angle,
      bool ccw
   )
   {
      auto start = start_angle * 180 / M_PI;
      auto sweep = (end_angle - start_angle) * 180 / M_PI;
      if (!ccw)
         sweep = -sweep;

      _state->path().addArc(
         { p.x-radius, p.y-radius, p.x+radius, p.y+radius },
         start, sweep
      );
   }

   void canvas::rect(struct rect r)
   {
      _state->path().addRect({ r.left, r.top, r.right, r.bottom });
   }

   void canvas::round_rect(struct rect r, float radius)
   {
      _state->path().addRoundRect({ r.left, r.top, r.right, r.bottom }, radius, radius);
   }

#if defined(ARTIST_SKIA)
   void canvas::circle(struct circle c)
   {
      _state->path().addCircle(c.cx, c.cy, c.radius);
   }
#endif

   void canvas::quadratic_curve_to(point cp, point end)
   {
      _state->path().quadTo(cp.x, cp.y, end.x, end.y);
   }

   void canvas::bezier_curve_to(point cp1, point cp2, point end)
   {
      _state->path().cubicTo(cp1.x, cp1.y, cp2.x, cp2.y, end.x, end.y);
   }

   void canvas::fill_style(color c)
   {
      _state->fill_paint().setColor4f({ c.red, c.green, c.blue, c.alpha }, nullptr);
      _state->fill_paint().setShader(nullptr);
   }

   void canvas::stroke_style(color c)
   {
      _state->stroke_paint().setColor4f({ c.red, c.green, c.blue, c.alpha }, nullptr);
      _state->stroke_paint().setShader(nullptr);
   }

   void canvas::line_width(float w)
   {
      _state->stroke_paint().setStrokeWidth(w);
   }

   void canvas::line_cap(line_cap_enum cap_)
   {
      SkPaint::Cap cap = SkPaint::kButt_Cap;
      switch (cap_)
      {
         case line_cap_enum::butt:     cap = SkPaint::kButt_Cap; break;
         case line_cap_enum::round:    cap = SkPaint::kRound_Cap; break;
         case line_cap_enum::square:   cap = SkPaint::kSquare_Cap; break;
      }
      _state->stroke_paint().setStrokeCap(cap);
   }

   void canvas::line_join(join_enum join_)
   {
      SkPaint::Join join = SkPaint::kMiter_Join;
      switch (join_)
      {
         case join_enum::bevel_join:   join = SkPaint::kBevel_Join; break;
         case join_enum::round_join:   join = SkPaint::kRound_Join; break;
         case join_enum::miter_join:   join = SkPaint::kMiter_Join; break;
      }
      _state->stroke_paint().setStrokeJoin(join);
   }

   void canvas::miter_limit(float limit)
   {
   }

   void canvas::shadow_style(point offset, float blur, color c)
   {
      constexpr auto blur_factor = 0.4f;

      auto matrix = _context->getTotalMatrix();
      auto scx = matrix.getScaleX() / _state->_pre_scale.x;
      auto scy = matrix.getScaleY() / _state->_pre_scale.y;

      auto shadow = SkDropShadowImageFilter::Make(
         offset.x / scx
       , offset.y / scy
       , (blur * blur_factor) / scx
       , (blur * blur_factor) / scy
       , SkColor4f{ c.red, c.green, c.blue, c.alpha }.toSkColor()
       , SkDropShadowImageFilter::kDrawShadowAndForeground_ShadowMode
       , nullptr
      );

      _state->stroke_paint().setImageFilter(shadow);
      _state->fill_paint().setImageFilter(shadow);
   }

   void canvas::global_composite_operation(composite_op_enum mode)
   {
      SkBlendMode mode_;
      switch (mode)
      {
         case source_over:       mode_ = SkBlendMode::kSrcOver;   break;
         case source_atop:       mode_ = SkBlendMode::kSrcATop;   break;
         case source_in:         mode_ = SkBlendMode::kSrcATop;   break;
         case source_out:        mode_ = SkBlendMode::kSrcOut;     break;
         case destination_over:  mode_ = SkBlendMode::kDstOver;   break;
         case destination_atop:  mode_ = SkBlendMode::kDstATop;   break;
         case destination_in:    mode_ = SkBlendMode::kDstIn;     break;
         case destination_out:   mode_ = SkBlendMode::kDstOut;    break;
         case lighter:           mode_ = SkBlendMode::kLighten;   break;
         case darker:            mode_ = SkBlendMode::kDarken;    break;
         case copy:              mode_ = SkBlendMode::kSrc;       break;
         case xor_:              mode_ = SkBlendMode::kXor;       break;
      };
      _state->stroke_paint().setBlendMode(mode_);
      _state->fill_paint().setBlendMode(mode_);
   }

   namespace
   {
      void convert_gradient(
         canvas::gradient const& gr
       , std::vector<SkColor4f>& colors_
       , std::vector<SkScalar>& pos
      )
      {
         for (auto const& ccs : gr.color_space)
         {
            colors_.push_back(
               SkColor4f{
                  ccs.color.red
                 , ccs.color.green
                 , ccs.color.blue
                 , ccs.color.alpha
               }
            );
            pos.push_back(ccs.offset);
         }
      }

      void set_linear(canvas::linear_gradient const& gr, SkPaint& paint)
      {
         SkPoint points[2] = {
            { gr.start.x, gr.start.y },
            { gr.end.x, gr.end.y }
         };
         std::vector<SkColor4f> colors_;
         std::vector<SkScalar> pos;
         convert_gradient(gr, colors_, pos);
         paint.setShader(
            SkGradientShader::MakeLinear(
               points, colors_.data()
             , SkColorSpace::MakeSRGB()->makeLinearGamma()
             , pos.data(), colors_.size()
             , SkTileMode::kClamp, 0, nullptr
            ));
      }

      void set_radial(canvas::radial_gradient const& gr, SkPaint& paint)
      {
         std::vector<SkColor4f> colors_;
         std::vector<SkScalar> pos;
         convert_gradient(gr, colors_, pos);
         paint.setShader(
            SkGradientShader::MakeTwoPointConical(
               { gr.c1.x, gr.c1.y }, gr.c1_radius
             , { gr.c2.x, gr.c2.y }, gr.c2_radius
             , colors_.data()
             , SkColorSpace::MakeSRGB()->makeLinearGamma()
             , pos.data(), colors_.size()
             , SkTileMode::kClamp, 0, nullptr
            ));
      }
   }

   void canvas::fill_style(linear_gradient const& gr)
   {
      set_linear(gr, _state->fill_paint());
   }

   void canvas::fill_style(radial_gradient const& gr)
   {
      set_radial(gr, _state->fill_paint());
   }

   void canvas::stroke_style(linear_gradient const& gr)
   {
      set_linear(gr, _state->stroke_paint());
   }

   void canvas::stroke_style(radial_gradient const& gr)
   {
      set_radial(gr, _state->stroke_paint());
   }

   void canvas::font(class font const& font_)
   {
      _state->font() = font_;
   }

   namespace
   {
      void prepare_text(
         font const& font
       , int text_align
       , point& p, char const* f, char const* l
      )
      {
         auto metrics = font.metrics();
         auto line_height = metrics.ascent + metrics.descent + metrics.leading;
         auto width = font.measure_text(std::string_view(f, l-f));
         switch (text_align & 0x1C)
         {
            case canvas::top:    p.y += metrics.ascent; break;
            case canvas::middle: p.y += (metrics.ascent - metrics.descent)/2; break;
            case canvas::bottom: p.y -= metrics.descent; break;
            default: break;
         }

         switch (text_align & 0x3)
         {
            case canvas::center: p.x -= width/2; break;
            case canvas::right:  p.x -= width; break;
            default: break;
         }
      }
   }

   void canvas::fill_text(std::string_view utf8, point p)
   {
      auto text_blob = SkTextBlob::MakeFromText(
         utf8.data(), utf8.size(), *_state->font().impl().get()
      );
      prepare_text(_state->font(), _state->text_align(), p, utf8.begin(), utf8.end());
      _context->drawTextBlob(text_blob.get(), p.x, p.y, _state->fill_paint());
   }

   void canvas::stroke_text(std::string_view utf8, point p)
   {
      auto text_blob = SkTextBlob::MakeFromText(
         utf8.data(), utf8.size(), *_state->font().impl().get()
      );
      prepare_text(_state->font(), _state->text_align(), p, utf8.begin(), utf8.end());
      _context->drawTextBlob(text_blob.get(), p.x, p.y, _state->stroke_paint());
   }

   canvas::text_metrics canvas::measure_text(std::string_view utf8)
   {
      return {};
   }

   void canvas::text_align(int align)
   {
      _state->text_align() = align;
   }

   void canvas::draw(picture const& pic, struct rect src, struct rect dest)
   {
      auto draw_picture =
         [&](auto const& that)
         {
            using T = std::decay_t<decltype(that)>;
            if constexpr(std::is_same_v<T, extent>)
            {
            }
            if constexpr(std::is_same_v<T, sk_sp<SkPicture>>)
            {
               SkMatrix mat;
               _context->drawPicture(that, &mat, nullptr);
               // $$$ JDG: Fixme. Compute the matrix $$$
            }
            if constexpr(std::is_same_v<T, SkBitmap>)
            {
               _context->drawBitmapRect(
                  that,
                  SkRect{ src.left, src.top, src.right, src.bottom },
                  SkRect{ dest.left, dest.top, dest.right, dest.bottom },
                  nullptr
               );
            }
         };

      return std::visit(draw_picture, *pic.impl());
   }
}
