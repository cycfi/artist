/*=============================================================================
   Copyright (c) 2016-2023 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <artist/image.hpp>

#include "SkBitmap.h"
#include "SkCodec.h"
#include "SkData.h"
#include "SkImage.h"
#include "SkPicture.h"
#include "SkSurface.h"
#include "SkPixmap.h"
#include <ganesh/SkSurfaceGanesh.h>
#include <encode/SkPngEncoder.h>
#include "SkCanvas.h"
#include "SkStream.h"

#include "opaque.hpp"
#include <stdexcept>
#include <string>
#include <tuple>

namespace cycfi::artist
{
   namespace
   {
      // Source pixel layout for make_image, plus its byte size per pixel.
      struct src_desc { SkColorType color; SkAlphaType alpha; int bpp; };

      src_desc map_src_fmt(pixel_format fmt)
      {
         switch (fmt)
         {
            case pixel_format::gray8:
               return {kGray_8_SkColorType,   kOpaque_SkAlphaType,   1};
            case pixel_format::rgb16:
               return {kRGB_565_SkColorType,  kOpaque_SkAlphaType,   2};
            case pixel_format::rgb32:
               return {kRGB_888x_SkColorType, kOpaque_SkAlphaType,   4};
            case pixel_format::rgba32:
               // Straight alpha in; premultiplied on the way into the bitmap.
               return {kRGBA_8888_SkColorType, kUnpremul_SkAlphaType, 4};
            default:
               return {kUnknown_SkColorType,  kUnknown_SkAlphaType,  0};
         }
      }
   }

   namespace
   {
      // The owned bitmap layout, uniform across backends: premultiplied B,G,R,A
      // in memory. kN32 is kRGBA_8888 on this platform, so pin kBGRA_8888
      // explicitly rather than relying on kN32.
      SkImageInfo bgra_premul(int w, int h)
      {
         return SkImageInfo::Make(w, h, kBGRA_8888_SkColorType, kPremul_SkAlphaType);
      }
   }

   // Every image owns a premultiplied-BGRA SkBitmap from construction. A blank
   // image is transparent pixels (never a null bitmap). size() is logical
   // units; bitmap_size() is pixels; scale() is pixels per logical unit.
   image::image(extent size, float scale)
    : _impl{new artist::image_impl(SkBitmap{})}
   {
      int w = int(size.x * scale + 0.5f);
      int h = int(size.y * scale + 0.5f);
      auto& bitmap = std::get<SkBitmap>(*_impl);
      if (!bitmap.tryAllocPixels(bgra_premul(w, h)))
         throw std::runtime_error{"artist skia backend: Failed to create image."};
      bitmap.eraseColor(SK_ColorTRANSPARENT);
      _impl->scale = scale;
   }

   float image::scale() const
   {
      return _impl ? _impl->scale : 1.0f;
   }

   image::image(fs::path const& path_)
    : _impl{new artist::image_impl(SkBitmap{})}
   {
      auto path = find_file(path_);
      auto fail = [&path_]()
      {
         throw std::runtime_error{"artist skia backend: Failed to load file: " + path_.string()};
      };

      sk_sp<SkData> data{SkData::MakeFromFileName(path.string().c_str())};
      std::unique_ptr<SkCodec> codec = SkCodec::MakeFromData(data);
      if (!codec)
         fail();
      SkImageInfo info = codec->getInfo()
         .makeColorType(kBGRA_8888_SkColorType)
         .makeAlphaType(kPremul_SkAlphaType);

      auto& bitmap = std::get<SkBitmap>(*_impl);
      if (!bitmap.tryAllocPixels(info))
         fail();

      if (codec->getPixels(info, bitmap.getPixels(), bitmap.rowBytes()) != SkCodec::kSuccess)
         fail();
   }

   image::image(uint8_t const* data, pixel_format fmt, extent size)
    : _impl{new artist::image_impl(SkBitmap{})}
   {
      auto src = map_src_fmt(fmt);
      if (src.bpp == 0)
         throw std::runtime_error{"artist skia backend: make_image: invalid pixel format."};

      int w = int(size.x);
      int h = int(size.y);

      // Wrap the caller's buffer, then convert into the owned N32-premul bitmap.
      SkImageInfo src_info = SkImageInfo::Make(w, h, src.color, src.alpha);
      SkPixmap src_pixmap{src_info, data, size_t(w) * src.bpp};

      auto& bitmap = std::get<SkBitmap>(*_impl);
      if (!bitmap.tryAllocPixels(bgra_premul(w, h)))
         throw std::runtime_error{"artist skia backend: make_image: failed to allocate."};
      if (!bitmap.writePixels(src_pixmap, 0, 0))
         throw std::runtime_error{"artist skia backend: make_image: pixel conversion failed."};
   }

   image::~image()
   {
      delete _impl;
   }

   image_impl_ptr image::impl() const
   {
      return _impl;
   }

   extent image::size() const
   {
      if (!_impl) return {};
      auto const& bitmap = std::get<SkBitmap>(_impl->base());
      float s = _impl->scale;
      return extent{float(bitmap.width()) / s, float(bitmap.height()) / s};
   }

   void image::save_png(std::string_view path_) const
   {
      std::string path{path_};
      auto fail = [&path]()
      {
         throw std::runtime_error{"artist skia backend: Failed to save file: " + path};
      };

      auto const& bitmap = std::get<SkBitmap>(_impl->base());
      sk_sp<SkImage> image = bitmap.asImage();
      if (!image)
         fail();

      sk_sp<SkData> png(SkPngEncoder::Encode(nullptr, image.get(), {}));
      if (!png)
         fail();

      SkFILEWStream out(path.c_str());
      if (!out.isValid() || !out.write(png->data(), png->size()))
         fail();
   }

   uint32_t* image::pixels()
   {
      if (!_impl) return nullptr;
      return reinterpret_cast<uint32_t*>(std::get<SkBitmap>(_impl->base()).getPixels());
   }

   uint32_t const* image::pixels() const
   {
      if (!_impl) return nullptr;
      return reinterpret_cast<uint32_t const*>(std::get<SkBitmap>(_impl->base()).getPixels());
   }

   extent image::bitmap_size() const
   {
      if (!_impl) return {};
      auto const& bitmap = std::get<SkBitmap>(_impl->base());
      return extent{float(bitmap.width()), float(bitmap.height())};
   }

   size_t image::_pixmap_size(pixel_format fmt, extent size)
   {
      return size_t(size.x) * size_t(size.y) * map_src_fmt(fmt).bpp;
   }

   ////////////////////////////////////////////////////////////////////////////
   // offscreen_image draws directly into the image's own bitmap: the drawing
   // is in the image immediately and earlier contents are kept. The scale CTM
   // lets draw code stay in logical coordinates. The result is always a
   // SkBitmap, so canvas::draw() takes the drawImageRect path, which honours
   // all SkBlendMode values.
   struct offscreen_image::state
   {
      std::unique_ptr<SkCanvas> canvas;
   };

   offscreen_image::offscreen_image(image& img)
    : _image{img}
    , _state{new offscreen_image::state{}}
   {
      auto& bitmap = std::get<SkBitmap>(img.impl()->base());
      _state->canvas = std::make_unique<SkCanvas>(bitmap);
      float s = img.scale();
      _state->canvas->scale(s, s);
   }

   offscreen_image::~offscreen_image()
   {
      delete _state;
   }

   canvas_impl* offscreen_image::context() const
   {
      return _state->canvas.get();
   }
}
