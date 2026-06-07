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
#include <stdexcept>
#include <string>

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

      IWICBitmap*       bitmap = nullptr;
      IWICBitmapLock*   _lock = nullptr;   // held for the lifetime once pixels() is called
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

   image::image(extent size)
    : _impl(new image_impl)
   {
      _impl->bitmap = make_wic_bitmap(
         UINT(size.x < 1? 1 : size.x), UINT(size.y < 1? 1 : size.y));
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
      // Only rgba32/rgb32 (32bpp) are wired up for now; others land with the
      // rest of the pixel-format matrix in a later pass.
      UINT stride = w * 4;
      UINT buf = stride * h;
      auto hr = d2d::get_wic_factory().CreateBitmapFromMemory(
         w, h, GUID_WICPixelFormat32bppPBGRA, stride, buf,
         const_cast<BYTE*>(data), &_impl->bitmap
      );
      if (!SUCCEEDED(hr))
         throw std::runtime_error{"Error: WIC CreateBitmapFromMemory failed."};
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
         return p? p->bitmap : nullptr;
      }
   }

   extent image::size() const
   {
      if (!_impl || !_impl->bitmap)
         return {};
      UINT w = 0, h = 0;
      _impl->bitmap->GetSize(&w, &h);
      return {float(w), float(h)};
   }

   extent image::bitmap_size() const
   {
      return size();
   }

   uint32_t* image::pixels()
   {
      if (!_impl || !_impl->bitmap)
         return nullptr;
      if (!_impl->_lock)
      {
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
      if (!_impl || !_impl->bitmap)
         return;

      fs::path p{std::string{path_}};
      // Ensure the destination directory exists (golden bootstrap writes into
      // dirs that may not be present yet).
      std::error_code ec;
      if (p.has_parent_path())
         fs::create_directories(p.parent_path(), ec);
      std::wstring wpath = p.wstring();

      // Every step is checked: WIC returns failure HRESULTs (e.g. an unwritable
      // path) rather than throwing, and the previous unchecked chain
      // null-dereferenced the next interface (crash at frame->Initialize).
      IWICStream* stream = nullptr;
      IWICBitmapEncoder* encoder = nullptr;
      IWICBitmapFrameEncode* frame = nullptr;
      auto& wic = d2d::get_wic_factory();

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
            encoder->Commit();
      }

      d2d::release(frame);
      d2d::release(encoder);
      d2d::release(stream);
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
      auto props = D2D1::RenderTargetProperties(
         D2D1_RENDER_TARGET_TYPE_DEFAULT,
         D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
      );
      auto hr = d2d::get_factory().CreateWicBitmapRenderTarget(
         img.impl()->bitmap, props, &_state->rt
      );
      if (!SUCCEEDED(hr))
         throw std::runtime_error{"Error: CreateWicBitmapRenderTarget failed."};
      _state->ctx.target(_state->rt);
      _state->rt->BeginDraw();
   }

   offscreen_image::~offscreen_image()
   {
      if (_state)
      {
         if (_state->rt)
         {
            _state->rt->EndDraw();
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
