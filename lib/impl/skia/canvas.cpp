/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <artist/canvas.hpp>
#include <stack>
#include <variant>
#include "opaque.hpp"

#include "SkBitmap.h"
#include "SkData.h"
#include "SkImage.h"
#include "SkPicture.h"
#include "SkSurface.h"
#include "SkCanvas.h"
#include "SkPath.h"

namespace cycfi::artist
{
   struct canvas::canvas_state
   {
      canvas_state()
      {
         fill_paint.setAntiAlias(true);
         fill_paint.setStyle(SkPaint::kFill_Style);
      }

      SkPath   path;
      SkPaint  fill_paint;
   };

   canvas::canvas(host_context_ptr context_)
    : _context{ context_ }
    , _state{ std::make_unique<canvas_state>() }
   {
   }

   canvas::~canvas()
   {
   }

   void canvas::translate(point p)
   {
   }

   void canvas::rotate(float rad)
   {
   }

   void canvas::scale(point p)
   {
   }

   void canvas::save()
   {
   }

   void canvas::restore()
   {
   }

   void canvas::begin_path()
   {
   }

   void canvas::close_path()
   {
   }

   void canvas::fill()
   {
      auto sk_canvas = (SkCanvas*)_context;
      sk_canvas->drawPath(_state->path, _state->fill_paint);
   }

   void canvas::fill_preserve()
   {
   }

   void canvas::stroke()
   {
   }

   void canvas::stroke_preserve()
   {
   }

   void canvas::clip()
   {
   }

   void canvas::move_to(point p)
   {
      _state->path.moveTo(p.x, p.y);
   }

   void canvas::line_to(point p)
   {
      _state->path.lineTo(p.x, p.y);
   }

   void canvas::arc_to(point p1, point p2, float radius)
   {
   }

   void canvas::arc(
      point p, float radius,
      float start_angle, float end_angle,
      bool ccw
   )
   {
   }

   void canvas::rect(struct rect r)
   {
      _state->path.addRect({ r.left, r.top, r.right, r.bottom });
   }

   void canvas::round_rect(struct rect r, float radius)
   {
   }

   void canvas::quadratic_curve_to(point cp, point end)
   {
   }

   void canvas::bezier_curve_to(point cp1, point cp2, point end)
   {
   }

   void canvas::fill_style(color c)
   {
      _state->fill_paint.setColor4f({ c.red, c.green, c.blue, c.alpha }, nullptr);
   }

   void canvas::stroke_style(color c)
   {
   }

   void canvas::line_width(float w)
   {
   }

   void canvas::line_cap(line_cap_enum cap_)
   {
   }

   void canvas::line_join(join_enum join_)
   {
   }

   void canvas::miter_limit(float limit)
   {
   }

   void canvas::shadow_style(point offset, float blur, color c)
   {
   }

   void canvas::global_composite_operation(composite_op_enum mode)
   {
   }

   void canvas::fill_style(linear_gradient const& gr)
   {
   }

   void canvas::fill_style(radial_gradient const& gr)
   {
   }

   void canvas::stroke_style(linear_gradient const& gr)
   {
   }

   void canvas::stroke_style(radial_gradient const& gr)
   {
   }

   void canvas::font(class font const& font_)
   {
   }

   void canvas::fill_text(std::string_view utf8, point p)
   {
   }

   void canvas::stroke_text(std::string_view utf8, point p)
   {
   }

   canvas::text_metrics canvas::measure_text(std::string_view utf8)
   {
      return {};
   }

   void canvas::text_align(int align)
   {
   }

   void canvas::draw(picture const& pic, struct rect src, struct rect dest)
   {
      auto sk_canvas = (SkCanvas*)_context;
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
               sk_canvas->drawPicture(that, &mat, nullptr);
               // $$$ JDG: Fixme. Compute the matrix $$$
            }
            if constexpr(std::is_same_v<T, SkBitmap>)
            {
               sk_canvas->drawBitmapRect(
                  that,
                  SkRect{src.left, src.right, src.top, src.bottom },
                  SkRect{dest.left, dest.right, dest.top, dest.bottom },
                  nullptr
               );
            }
         };

      return std::visit(draw_picture, *pic.host_picture());
   }
}
