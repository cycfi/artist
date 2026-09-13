/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]

   Images in the recording backend have real dimensions and a real pixel
   buffer, so the image API behaves as it does everywhere: sizes, scale,
   pixel formats and PNG output all hold. Drawing into an image records into
   its journal and does not touch the pixels. Loading reads only the size
   from PNG, JPEG and WebP headers; the pixels start transparent.
=============================================================================*/
#include "recording_impl.hpp"
#include <artist/image.hpp>
#include <artist/resources.hpp>
#include <algorithm>
#include <array>
#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>

namespace cycfi::artist
{
   class image_impl
   {
   public:

      extent                  size = {};        // units
      float                   scale = 1;
      std::size_t             width = 0;        // pixels
      std::size_t             height = 0;
      std::vector<uint32_t>   pixels;
      recording::journal      journal;

      void allocate(std::size_t w, std::size_t h, float scale_)
      {
         width = w;
         height = h;
         scale = scale_;
         size = {float(w) / scale_, float(h) / scale_};
         pixels.assign(w * h, 0);
      }
   };

   namespace
   {
      ////////////////////////////////////////////////////////////////////////
      // Image headers: just enough to learn the size.
      bool read_dimensions(std::vector<uint8_t> const& d, uint32_t& w, uint32_t& h)
      {
         auto be16 = [&](std::size_t i){ return uint32_t(d[i]) << 8 | d[i + 1]; };
         auto be32 = [&](std::size_t i){ return be16(i) << 16 | be16(i + 2); };
         auto le16 = [&](std::size_t i){ return uint32_t(d[i]) | uint32_t(d[i + 1]) << 8; };
         auto le24 = [&](std::size_t i){ return le16(i) | uint32_t(d[i + 2]) << 16; };
         auto le32 = [&](std::size_t i){ return le16(i) | le16(i + 2) << 16; };

         static constexpr uint8_t png_signature[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
         if (d.size() >= 24 && std::memcmp(d.data(), png_signature, 8) == 0)
         {
            w = be32(16);
            h = be32(20);
            return w && h;
         }

         if (d.size() >= 4 && d[0] == 0xFF && d[1] == 0xD8)
         {
            // Walk the markers to the first start-of-frame.
            std::size_t i = 2;
            while (i + 9 < d.size())
            {
               if (d[i] != 0xFF)
               {
                  ++i;
                  continue;
               }
               auto m = d[i + 1];
               if (m == 0xFF)
               {
                  ++i;
                  continue;
               }
               if (m == 0xD8 || m == 0x01 || (m >= 0xD0 && m <= 0xD7))
               {
                  i += 2;
                  continue;
               }
               bool sof = m >= 0xC0 && m <= 0xCF && m != 0xC4 && m != 0xC8 && m != 0xCC;
               if (sof)
               {
                  h = be16(i + 5);
                  w = be16(i + 7);
                  return w && h;
               }
               i += 2 + be16(i + 2);
            }
            return false;
         }

         if (d.size() >= 30 &&
            std::memcmp(d.data(), "RIFF", 4) == 0 && std::memcmp(d.data() + 8, "WEBP", 4) == 0)
         {
            if (std::memcmp(d.data() + 12, "VP8 ", 4) == 0)
            {
               w = le16(26) & 0x3FFF;
               h = le16(28) & 0x3FFF;
               return w && h;
            }
            if (std::memcmp(d.data() + 12, "VP8L", 4) == 0)
            {
               auto bits = le32(21);
               w = 1 + (bits & 0x3FFF);
               h = 1 + ((bits >> 14) & 0x3FFF);
               return true;
            }
            if (std::memcmp(d.data() + 12, "VP8X", 4) == 0)
            {
               w = 1 + le24(24);
               h = 1 + le24(27);
               return true;
            }
         }
         return false;
      }

      ////////////////////////////////////////////////////////////////////////
      // PNG output with stored (uncompressed) deflate blocks: a valid file
      // that needs neither zlib nor a codec.
      uint32_t crc32(uint8_t const* p, std::size_t n, uint32_t crc = 0)
      {
         static auto const table = []
         {
            std::array<uint32_t, 256> t;
            for (uint32_t i = 0; i != 256; ++i)
            {
               uint32_t c = i;
               for (int k = 0; k != 8; ++k)
                  c = (c & 1)? 0xEDB88320u ^ (c >> 1) : c >> 1;
               t[i] = c;
            }
            return t;
         }();

         crc = ~crc;
         for (std::size_t i = 0; i != n; ++i)
            crc = table[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
         return ~crc;
      }

      void put_be32(std::vector<uint8_t>& out, uint32_t v)
      {
         out.push_back(uint8_t(v >> 24));
         out.push_back(uint8_t(v >> 16));
         out.push_back(uint8_t(v >> 8));
         out.push_back(uint8_t(v));
      }

      void put_chunk(std::vector<uint8_t>& out, char const* type, std::vector<uint8_t> const& data)
      {
         put_be32(out, uint32_t(data.size()));
         auto start = out.size();
         out.insert(out.end(), type, type + 4);
         out.insert(out.end(), data.begin(), data.end());
         put_be32(out, crc32(out.data() + start, out.size() - start));
      }

      std::vector<uint8_t> encode_png(image_impl const& im)
      {
         // Scanlines of straight-alpha RGBA, each led by filter type 0.
         std::vector<uint8_t> raw;
         raw.reserve(im.height * (1 + 4 * im.width));
         for (std::size_t y = 0; y != im.height; ++y)
         {
            raw.push_back(0);
            for (std::size_t x = 0; x != im.width; ++x)
            {
               auto p = im.pixels[y * im.width + x];
               unsigned a = (p >> 24) & 0xFF;
               auto straight = [a](unsigned c)
               {
                  return a? uint8_t(std::min(255u, (c * 255 + a / 2) / a)) : uint8_t(0);
               };
               raw.push_back(straight((p >> 16) & 0xFF));
               raw.push_back(straight((p >> 8) & 0xFF));
               raw.push_back(straight(p & 0xFF));
               raw.push_back(uint8_t(a));
            }
         }

         std::vector<uint8_t> z{0x78, 0x01};
         std::size_t pos = 0;
         do
         {
            auto n = std::min<std::size_t>(65535, raw.size() - pos);
            bool last = pos + n == raw.size();
            auto len = uint16_t(n);
            auto nlen = uint16_t(~len);
            z.push_back(last? 1 : 0);
            z.push_back(uint8_t(len));
            z.push_back(uint8_t(len >> 8));
            z.push_back(uint8_t(nlen));
            z.push_back(uint8_t(nlen >> 8));
            z.insert(z.end(), raw.begin() + pos, raw.begin() + pos + n);
            pos += n;
         }
         while (pos < raw.size());

         uint32_t s1 = 1, s2 = 0;
         for (auto c : raw)
         {
            s1 = (s1 + c) % 65521;
            s2 = (s2 + s1) % 65521;
         }
         put_be32(z, (s2 << 16) | s1);

         std::vector<uint8_t> ihdr;
         put_be32(ihdr, uint32_t(im.width));
         put_be32(ihdr, uint32_t(im.height));
         ihdr.insert(ihdr.end(), {8, 6, 0, 0, 0});   // 8 bits, RGBA

         std::vector<uint8_t> png{0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
         put_chunk(png, "IHDR", ihdr);
         put_chunk(png, "IDAT", z);
         put_chunk(png, "IEND", {});
         return png;
      }
   }

   ////////////////////////////////////////////////////////////////////////////
   // image
   ////////////////////////////////////////////////////////////////////////////
   image::image(extent size, float scale)
    : _impl(new image_impl)
   {
      float const w = size.x * scale;
      float const h = size.y * scale;
      _impl->allocate(
         std::size_t(w < 1? 1 : w + 0.5f), std::size_t(h < 1? 1 : h + 0.5f), scale);
   }

   image::image(fs::path const& path_)
    : _impl(nullptr)
   {
      auto full = find_file(path_);
      if (full.empty())
         throw std::runtime_error{
            "artist recording backend: File does not exist: " + path_.string()};

      std::ifstream f{full, std::ios::binary};
      std::vector<uint8_t> bytes{std::istreambuf_iterator<char>{f}, {}};
      uint32_t w = 0, h = 0;
      if (!read_dimensions(bytes, w, h))
         throw std::runtime_error{
            "artist recording backend: Unsupported image: " + full.string()};

      _impl = new image_impl;
      _impl->allocate(w, h, 1);
   }

   image::image(uint8_t const* data, pixel_format fmt, extent size)
    : _impl(nullptr)
   {
      if (fmt == pixel_format::invalid)
         throw std::runtime_error{"Error: Cannot initialize format: INVALID"};

      _impl = new image_impl;
      auto w = std::size_t(size.x), h = std::size_t(size.y);
      _impl->allocate(w, h, 1);

      // Into premultiplied B, G, R, A: the layout every backend uses.
      auto put = [this](std::size_t i, uint32_t r, uint32_t g, uint32_t b, uint32_t a)
      {
         _impl->pixels[i] = a << 24 | r << 16 | g << 8 | b;
      };
      auto n = w * h;
      switch (fmt)
      {
         case pixel_format::gray8:
            for (std::size_t i = 0; i != n; ++i)
               put(i, data[i], data[i], data[i], 255);
            break;

         case pixel_format::rgb16:
            for (std::size_t i = 0; i != n; ++i)
            {
               uint32_t v = data[i * 2] | (uint32_t(data[i * 2 + 1]) << 8);
               uint32_t r = (v >> 11) & 0x1F, g = (v >> 5) & 0x3F, b = v & 0x1F;
               put(i, (r * 255 + 15) / 31, (g * 255 + 31) / 63, (b * 255 + 15) / 31, 255);
            }
            break;

         case pixel_format::rgb32:
            for (std::size_t i = 0; i != n; ++i)
               put(i, data[i * 4], data[i * 4 + 1], data[i * 4 + 2], 255);
            break;

         default:
            for (std::size_t i = 0; i != n; ++i)
            {
               uint32_t a = data[i * 4 + 3];
               auto pm = [a](uint32_t c){ return (c * a + 127) / 255; };
               put(i, pm(data[i * 4]), pm(data[i * 4 + 1]), pm(data[i * 4 + 2]), a);
            }
            break;
      }
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
      return _impl? _impl->size : extent{};
   }

   float image::scale() const
   {
      return _impl? _impl->scale : 1.0f;
   }

   extent image::bitmap_size() const
   {
      if (!_impl)
         return {};
      return {float(_impl->width), float(_impl->height)};
   }

   uint32_t* image::pixels()
   {
      return (_impl && !_impl->pixels.empty())? _impl->pixels.data() : nullptr;
   }

   uint32_t const* image::pixels() const
   {
      return (_impl && !_impl->pixels.empty())? _impl->pixels.data() : nullptr;
   }

   void image::save_png(std::string_view path_) const
   {
      auto fail = [&path_]()
      {
         throw std::runtime_error{
            "artist recording backend: Failed to save file: " + std::string{path_}};
      };

      if (!_impl)
         fail();
      std::ofstream f{std::string{path_}, std::ios::binary};
      if (!f)
         fail();
      auto bytes = encode_png(*_impl);
      f.write(reinterpret_cast<char const*>(bytes.data()), std::streamsize(bytes.size()));
      if (!f)
         fail();
   }

   size_t image::_pixmap_size(pixel_format fmt, extent size)
   {
      size_t n = size_t(size.x) * size_t(size.y);
      switch (fmt)
      {
         case pixel_format::gray8:  return n;
         case pixel_format::rgb16:  return n * 2;
         case pixel_format::rgb32:
         case pixel_format::rgba32: return n * 4;
         default:                   return 0;
      }
   }

   ////////////////////////////////////////////////////////////////////////////
   // offscreen_image
   ////////////////////////////////////////////////////////////////////////////
   struct offscreen_image::state
   {
      recording::canvas_impl  ctx;
   };

   offscreen_image::offscreen_image(image& img)
    : _image{img}
    , _state{new state}
   {
      if (auto p = img.impl())
      {
         _state->ctx.out = &p->journal;
         _state->ctx.size = p->size;
      }
   }

   offscreen_image::~offscreen_image()
   {
      delete _state;
   }

   canvas_impl* offscreen_image::context() const
   {
      return &_state->ctx;
   }
}

namespace cycfi::artist::recording
{
   journal& journal_of(image& img)
   {
      return img.impl()->journal;
   }

   journal const& journal_of(image const& img)
   {
      return img.impl()->journal;
   }
}
