/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <artist/image.hpp>

#include "SkBitmap.h"
#include "SkCodec.h"
#include "SkData.h"
#include "SkImage.h"
#include "SkPicture.h"
#include "SkSurface.h"
#include "SkCanvas.h"
#include "SkPictureRecorder.h"

#include "opaque.hpp"
#include <stdexcept>
#include <map>
#include <string>
#include <utility> // std::pair
#include <iostream>

using std::map;
using std::pair;

namespace cycfi::artist
{
   pair<SkAlphaType, SkColorType> _map_img_fmt_to_api_type(const pixel_format& fmt)
   {
      switch(fmt)
      {
         case pixel_format::gray8:
            return {SkAlphaType::kOpaque_SkAlphaType, SkColorType::kGray_8_SkColorType};
         case pixel_format::rgb16:
            return {SkAlphaType::kOpaque_SkAlphaType, SkColorType::kRGB_565_SkColorType};
         case pixel_format::rgb32:
            return {SkAlphaType::kOpaque_SkAlphaType, SkColorType::kRGB_888x_SkColorType};
         case pixel_format::rgba32:
            return {SkAlphaType::kOpaque_SkAlphaType, SkColorType::kRGBA_8888_SkColorType};
         default:
            return {SkAlphaType::kUnknown_SkAlphaType, SkColorType::kUnknown_SkColorType};
      }
   }

   image::image(extent size)
    : _impl{ new artist::image_impl(size) }
   {}

   image::image(fs::path const& path_)
    : _impl{ new artist::image_impl(SkBitmap{}) }
   {
      auto path = find_file(path_);
      auto fail = [&path_]()
      {
         throw std::runtime_error{ "Error: Failed to load file: " + path_.string() };
      };

      sk_sp<SkData> data{ SkData::MakeFromFileName(path.string().c_str()) };
      std::unique_ptr<SkCodec> codec = SkCodec::MakeFromData(data);
      if (!codec)
         fail();
      SkImageInfo info = codec->getInfo().makeColorType(kN32_SkColorType);

      auto& bitmap = std::get<SkBitmap>(*_impl);
      if (!bitmap.tryAllocPixels(info))
         fail();

      if (codec->getPixels(info, bitmap.getPixels(), bitmap.rowBytes()) != SkCodec::kSuccess)
         fail();
   }

   image::image(uint8_t* data, pixel_format fmt, extent size)
    : _impl{ new artist::image_impl(SkBitmap{}) }
   {
      if (fmt == pixel_format::invalid)
         throw std::runtime_error{ "Error: Cannot initalize format: INVALID" };

      SkAlphaType alpha_fmt;
      SkColorType byte_fmt;
      try {
         std::tie(alpha_fmt, byte_fmt) = _map_img_fmt_to_api_type(fmt);
      } catch(std::exception& /* e */) {
         throw std::runtime_error{ "Error: unrecognized format." };
      }

      auto& bitmap = std::get<SkBitmap>(*_impl);
      SkImageInfo skImgInfo = SkImageInfo::Make(size.x, size.y, byte_fmt, alpha_fmt);
      if (!bitmap.tryAllocPixels(skImgInfo))
         throw std::runtime_error{ "Error: Failed to initialize image from pixel buffer" };

      memcpy(bitmap.getPixels(), data, _pixmap_size(fmt, size));
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
      auto get_size =
         [](auto const& that) -> extent
         {
            using T = std::decay_t<decltype(that)>;
            if constexpr(std::is_same_v<T, extent>)
            {
               return that;
            }
            if constexpr(std::is_same_v<T, sk_sp<SkPicture>>)
            {
               auto r = that->cullRect();
               return extent{ r.width(), r.height() };
            }
            if constexpr(std::is_same_v<T, SkBitmap>)
            {
               return extent{ float(that.width()), float(that.height()) };
            }
         };

      return std::visit(get_size, _impl->base());
   }

   void image::save_png(std::string_view path_) const
   {
      std::string path{ path_ };
      auto fail = [&path]()
      {
         throw std::runtime_error{ "Error: Failed to save file: " + path };
      };

      auto size_ = size();
      sk_sp<SkSurface> surface = SkSurface::MakeRasterN32Premul(size_.x, size_.y);
      SkCanvas* sk_canvas = surface->getCanvas();

      auto draw_picture =
         [&](auto const& that)
         {
            using T = std::decay_t<decltype(that)>;
            if constexpr(std::is_same_v<T, extent>)
            {
            }
            if constexpr(std::is_same_v<T, sk_sp<SkPicture>>)
            {
               sk_canvas->drawPicture(that);
            }
            if constexpr(std::is_same_v<T, SkBitmap>)
            {
               sk_canvas->drawBitmap(that, 0, 0);
            }
         };

      std::visit(draw_picture, _impl->base());

      // Make a PNG encoded image using the canvas
      sk_sp<SkImage> image(surface->makeImageSnapshot());
      if (!image)
         fail();

      sk_sp<SkData> png(image->encodeToData());
      if (!png)
         fail();

      // write the data to the file specified by filePath
      SkFILEWStream out(path.c_str());
      out.write(png->data(), png->size());
   }

   uint32_t* image::pixels()
   {
      auto get_pixels =
         [&](auto const& that) -> uint32_t*
         {
            using T = std::decay_t<decltype(that)>;
            if constexpr(std::is_same_v<T, SkBitmap>)
               return reinterpret_cast<uint32_t*>(that.getPixels());
            else
               return nullptr;
         };

      return std::visit(get_pixels, _impl->base());
   }

   uint32_t const* image::pixels() const
   {
      auto get_pixels =
         [&](auto const& that) -> uint32_t const*
         {
            using T = std::decay_t<decltype(that)>;
            if constexpr(std::is_same_v<T, SkBitmap>)
               return reinterpret_cast<uint32_t const*>(that.getPixels());
            else
               return nullptr;
         };

      return std::visit(get_pixels, _impl->base());
   }

   extent image::bitmap_size() const
   {
      auto get_size =
         [&](auto const& that) -> extent
         {
            using T = std::decay_t<decltype(that)>;
            if constexpr(std::is_same_v<T, SkBitmap>)
               return extent{ float(that.width()), float(that.height()) };
            else
               return {};
         };

      return std::visit(get_size, _impl->base());
   }

   size_t image::_pixmap_size(pixel_format fmt, extent size)
   {
      size_t fmt_bytes_per_pixel = ([&fmt]() {
         switch (fmt) {
            case pixel_format::gray8:
               return 1;
            case pixel_format::rgb16:
               return 2;
            case pixel_format::rgb32:
            case pixel_format::rgba32:
               return 4;
            default:
               return 0;
         }
      })();
      return static_cast<size_t>(size.x) * static_cast<size_t>(size.y) * fmt_bytes_per_pixel;
   }

   struct offscreen_image::state
   {
      SkPictureRecorder recorder;
      SkCanvas* recording_canvas;
   };

   offscreen_image::offscreen_image(image& img)
    : _image{ img }
    , _state{ new offscreen_image::state{} }
   {
      auto size = _image.size();
      _state->recording_canvas = _state->recorder.beginRecording(size.x, size.y);
   }

   offscreen_image::~offscreen_image()
   {
      *(_image.impl()) = _state->recorder.finishRecordingAsPicture();
      delete _state;
   }

   canvas_impl* offscreen_image::context() const
   {
      return _state->recording_canvas;
   }
}

