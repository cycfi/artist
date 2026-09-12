/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]

   WIC-backed image + offscreen rendering. The offscreen_image creates an
   ID2D1RenderTarget over the image's IWICBitmap (CreateWicBitmapRenderTarget)
   wrapped in a d2d::context, so the canvas can render headless (this is what
   the test harness uses). Pixels are premultiplied BGRA (little-endian uint32
   == 0xAARRGGBB), matching the golden comparator's unpacking.
=============================================================================*/
#include <artist/image.hpp>
#include "context.hpp"
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

namespace cycfi::artist
{
   class image_impl
   {
   public:
      ~image_impl()
      {
         d2d::release(_lock);
         d2d::release(bitmap);
      }

      // A WIC bitmap render target holds the bitmap locked for writing between
      // BeginDraw and EndDraw, so Flush alone does not make the pixels
      // readable: reading or encoding them has to end the draw first. The
      // pixel lock from pixels() blocks the same users, so it goes too.
      void suspend()
      {
         if (rt && drawing)
         {
            rt->EndDraw();
            drawing = false;
         }
         d2d::release(_lock);
      }

      void resume()
      {
         if (rt && !drawing && !_lock)
         {
            rt->BeginDraw();
            drawing = true;
         }
      }

      IWICBitmap*       bitmap = nullptr;
      IWICBitmapLock*   _lock = nullptr;   // held from pixels() until suspend()
      ID2D1RenderTarget* rt = nullptr;     // non-owning: the live offscreen target
      bool              drawing = false;   // rt is between BeginDraw and EndDraw

      // The bitmap is allocated at size * scale pixels. size() reports logical
      // units, bitmap_size() reports pixels, and this is the ratio between them.
      float             scale = 1.0f;
   };

   namespace
   {
      IWICBitmap* make_wic_bitmap(UINT w, UINT h)
      {
         IWICBitmap* bm = nullptr;
         auto hr = d2d::get_wic_factory().CreateBitmap(
            w, h, GUID_WICPixelFormat32bppPBGRA,
            WICBitmapCacheOnDemand, &bm
         );
         if (!SUCCEEDED(hr))
            throw std::runtime_error{"Error: WIC CreateBitmap failed."};
         return bm;
      }
   }

   image::image(extent size, float scale)
    : _impl(new image_impl)
   {
      float const w = size.x * scale;
      float const h = size.y * scale;
      _impl->bitmap = make_wic_bitmap(
         UINT(w < 1? 1 : w + 0.5f), UINT(h < 1? 1 : h + 0.5f));
      _impl->scale = scale;
   }

   image::image(fs::path const& path_)
    : _impl(new image_impl)
   {
      auto fs_path = find_file(path_);
      std::wstring wpath = fs_path.wstring();

      IWICBitmapDecoder* decoder = nullptr;
      auto hr = d2d::get_wic_factory().CreateDecoderFromFilename(
         wpath.c_str(), nullptr, GENERIC_READ,
         WICDecodeMetadataCacheOnLoad, &decoder
      );
      if (!SUCCEEDED(hr))
         throw std::runtime_error{"Error: cannot open image file: " + fs_path.string()};

      IWICBitmapFrameDecode* frame = nullptr;
      decoder->GetFrame(0, &frame);

      IWICFormatConverter* converter = nullptr;
      d2d::get_wic_factory().CreateFormatConverter(&converter);
      converter->Initialize(
         frame, GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone,
         nullptr, 0.0, WICBitmapPaletteTypeCustom
      );

      // Materialize an editable copy (a converter is read-only / not a render
      // target source we can also lock).
      d2d::get_wic_factory().CreateBitmapFromSource(
         converter, WICBitmapCacheOnLoad, &_impl->bitmap
      );

      d2d::release(converter);
      d2d::release(frame);
      d2d::release(decoder);
   }

   image::image(uint8_t const* data, pixel_format fmt, extent size)
    : _impl(new image_impl)
   {
      if (fmt == pixel_format::invalid)
         throw std::runtime_error{"Error: Cannot initialize format: INVALID"};

      UINT w = UINT(size.x), h = UINT(size.y);
      std::size_t const n = std::size_t(w) * h;

      // Convert into the backend's own layout: premultiplied B, G, R, A. The
      // caller's buffer is copied, so it may be reused or destroyed after this.
      std::vector<uint8_t> px(n * 4);
      auto put =
         [&px](std::size_t i, uint32_t r, uint32_t g, uint32_t b, uint32_t a)
         {
            auto* d = px.data() + i * 4;
            d[0] = uint8_t(b);
            d[1] = uint8_t(g);
            d[2] = uint8_t(r);
            d[3] = uint8_t(a);
         };

      switch (fmt)
      {
         case pixel_format::gray8:
            for (std::size_t i = 0; i != n; ++i)
            {
               uint32_t v = data[i];
               put(i, v, v, v, 255);
            }
            break;

         case pixel_format::rgb16:
            // 5-6-5, red in the top five bits; each channel expanded so that
            // an all-ones field is 255.
            for (std::size_t i = 0; i != n; ++i)
            {
               uint32_t v = data[i * 2] | (uint32_t(data[i * 2 + 1]) << 8);
               uint32_t r = (v >> 11) & 0x1F;
               uint32_t g = (v >> 5) & 0x3F;
               uint32_t b = v & 0x1F;
               put(i, (r * 255 + 15) / 31, (g * 255 + 31) / 63, (b * 255 + 15) / 31, 255);
            }
            break;

         case pixel_format::rgb32:
            // R, G, B and an ignored fourth byte: opaque, nothing to scale.
            for (std::size_t i = 0; i != n; ++i)
               put(i, data[i * 4], data[i * 4 + 1], data[i * 4 + 2], 255);
            break;

         default:
         {
            // rgba32: straight alpha in, premultiplied out.
            for (std::size_t i = 0; i != n; ++i)
            {
               uint32_t a = data[i * 4 + 3];
               auto pm = [a](uint32_t c) { return (c * a + 127) / 255; };
               put(i, pm(data[i * 4]), pm(data[i * 4 + 1]), pm(data[i * 4 + 2]), a);
            }
            break;
         }
      }

      // Copy into a bitmap WIC owns, row by row: the destination stride is the
      // bitmap's own, which need not be w * 4.
      _impl->bitmap = make_wic_bitmap(w, h);
      WICRect rc{0, 0, INT(w), INT(h)};
      IWICBitmapLock* lock = nullptr;
      if (!SUCCEEDED(_impl->bitmap->Lock(&rc, WICBitmapLockWrite, &lock)) || !lock)
         throw std::runtime_error{"Error: WIC bitmap Lock failed."};

      UINT stride = 0, cb = 0;
      BYTE* dest = nullptr;
      lock->GetStride(&stride);
      lock->GetDataPointer(&cb, &dest);
      for (UINT y = 0; y != h; ++y)
         std::memcpy(dest + std::size_t(y) * stride, px.data() + std::size_t(y) * w * 4, w * 4);
      d2d::release(lock);
   }

   image::~image()
   {
      delete _impl;
   }

   image_impl_ptr image::impl() const
   {
      return _impl;
   }

   namespace d2d
   {
      IWICBitmap* wic_bitmap(image const& img)
      {
         auto p = img.impl();
         if (!p)
            return nullptr;
         p->suspend();   // the caller is about to draw from it
         return p->bitmap;
      }
   }

   extent image::size() const
   {
      // Logical units: the pixel dimensions divided by the device scale.
      auto const px = bitmap_size();
      auto const sc = scale();
      return {px.x / sc, px.y / sc};
   }

   float image::scale() const
   {
      return (_impl && _impl->scale > 0)? _impl->scale : 1.0f;
   }

   extent image::bitmap_size() const
   {
      // Physical pixel dimensions, always.
      if (!_impl || !_impl->bitmap)
         return {};
      UINT w = 0, h = 0;
      _impl->bitmap->GetSize(&w, &h);
      return {float(w), float(h)};
   }

   uint32_t* image::pixels()
   {
      if (!_impl || !_impl->bitmap)
         return nullptr;
      if (!_impl->_lock)
      {
         _impl->suspend();    // land any pending offscreen drawing
         UINT w = 0, h = 0;
         _impl->bitmap->GetSize(&w, &h);
         WICRect rc{0, 0, INT(w), INT(h)};
         if (!SUCCEEDED(_impl->bitmap->Lock(
               &rc, WICBitmapLockRead | WICBitmapLockWrite, &_impl->_lock)))
            return nullptr;
      }
      UINT cb = 0;
      BYTE* p = nullptr;
      _impl->_lock->GetDataPointer(&cb, &p);
      return reinterpret_cast<uint32_t*>(p);
   }

   uint32_t const* image::pixels() const
   {
      return const_cast<image*>(this)->pixels();
   }

   void image::save_png(std::string_view path_) const
   {
      auto fail = [&path_]()
      {
         throw std::runtime_error{
            "artist direct2d backend: Failed to save file: " + std::string{path_}};
      };

      if (!_impl || !_impl->bitmap)
         fail();
      _impl->suspend();

      fs::path p{std::string{path_}};
      std::wstring wpath = p.wstring();

      // Every step is checked: WIC returns failure HRESULTs (e.g. an unwritable
      // path) rather than throwing, and the previous unchecked chain
      // null-dereferenced the next interface (crash at frame->Initialize).
      IWICStream* stream = nullptr;
      IWICBitmapEncoder* encoder = nullptr;
      IWICBitmapFrameEncode* frame = nullptr;
      auto& wic = d2d::get_wic_factory();
      bool ok = false;

      if (SUCCEEDED(wic.CreateStream(&stream)) && stream &&
          SUCCEEDED(stream->InitializeFromFilename(wpath.c_str(), GENERIC_WRITE)) &&
          SUCCEEDED(wic.CreateEncoder(GUID_ContainerFormatPng, nullptr, &encoder)) && encoder &&
          SUCCEEDED(encoder->Initialize(stream, WICBitmapEncoderNoCache)) &&
          SUCCEEDED(encoder->CreateNewFrame(&frame, nullptr)) && frame &&
          SUCCEEDED(frame->Initialize(nullptr)))
      {
         UINT w = 0, h = 0;
         _impl->bitmap->GetSize(&w, &h);
         WICPixelFormatGUID pf = GUID_WICPixelFormat32bppPBGRA;
         frame->SetSize(w, h);
         frame->SetPixelFormat(&pf);
         if (SUCCEEDED(frame->WriteSource(_impl->bitmap, nullptr)) &&
             SUCCEEDED(frame->Commit()))
            ok = SUCCEEDED(encoder->Commit());
      }

      d2d::release(frame);
      d2d::release(encoder);
      d2d::release(stream);
      _impl->resume();     // an offscreen still drawing into this image

      if (!ok)
         fail();
   }

   ////////////////////////////////////////////////////////////////////////////
   // offscreen_image
   ////////////////////////////////////////////////////////////////////////////
   struct offscreen_image::state
   {
      d2d::context            ctx;
      ID2D1RenderTarget*      rt = nullptr;
   };

   offscreen_image::offscreen_image(image& img)
    : _image(img)
   {
      _state = new state;
      img.impl()->suspend();  // a locked bitmap cannot back a render target
      auto props = D2D1::RenderTargetProperties(
         D2D1_RENDER_TARGET_TYPE_DEFAULT,
         D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
      );
      auto hr = d2d::get_factory().CreateWicBitmapRenderTarget(
         img.impl()->bitmap, props, &_state->rt
      );
      if (!SUCCEEDED(hr))
         throw std::runtime_error{"Error: CreateWicBitmapRenderTarget failed."};

      // The bitmap is allocated at size * scale pixels, so the target has to
      // interpret drawing in logical units. Direct2D maps DIPs to pixels by
      // DPI, so a DPI of 96 * scale is the equivalent of Cairo's device scale.
      // It is not part of the transform stack, so canvas transforms do not
      // clobber it.
      float const sc = img.scale();
      if (sc > 0 && sc != 1.0f)
         _state->rt->SetDpi(96.0f * sc, 96.0f * sc);

      // save_png and pixels() end and restart the draw through this while the
      // offscreen is alive, so drawing reaches the image immediately rather
      // than at EndDraw.
      img.impl()->rt = _state->rt;

      _state->ctx.target(_state->rt);
      _state->rt->BeginDraw();
      img.impl()->drawing = true;
   }

   offscreen_image::~offscreen_image()
   {
      if (_state)
      {
         if (_state->rt)
         {
            if (auto p = _image.impl())
            {
               if (p->drawing)
                  _state->rt->EndDraw();
               p->drawing = false;
               p->rt = nullptr;
            }
            d2d::release(_state->rt);
         }
         delete _state;
      }
   }

   canvas_impl* offscreen_image::context() const
   {
      return &_state->ctx;
   }
}
