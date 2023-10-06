/*=============================================================================
   Copyright (c) 2016-2020 Brent Soles

   Distributed under the MIT License (https://opensource.org/licenses/MIT)
=============================================================================*/
#include "app.hpp"

using namespace cycfi::artist;
using cycfi::is_little_endian;

int constexpr rows = 8;
int constexpr cols = 8;
int constexpr square_side = 60;
int constexpr square_area = square_side * square_side;
int constexpr width = cols * square_side;
int constexpr height = rows * square_side;
size_t constexpr pix_buf_size = rows * cols * square_area;

uint32_t constexpr white = 0xffffffff;
// on little endian systems RGBA is formatted as ABGR for values
std::function<uint32_t()> black = []() { return is_little_endian() ? 0xff000000 : 0x000000ff; };

auto constexpr window_size = extent{
   static_cast<float>(cols * square_side),
   static_cast<float>(rows * square_side)
};

void draw(canvas& cnv)
{
   // Create a chess board
   std::unique_ptr<uint32_t> pix_buf(new uint32_t[pix_buf_size]);
   for (int y = 0; y < rows; y++)
   {
      uint32_t* row_slice = &(pix_buf.get()[y * rows * square_area]);
      uint32_t color = black();
      if (y % 2 != 0)
         color = white;
      for (int x = 0; x < cols * square_area; x++)
      {
         if (x % square_side == 0)
         {
            if (color == white)
               color = black();
            else
               color = white;
         }

         row_slice[x] = color;
      }
   }

   auto img = make_image<pixel_format::rgba32>(pix_buf.get(), { window_size.x, window_size.y });
   cnv.draw(img);
}

int main(int argc, char const* argv[])
{
   return run_app(argc, argv, window_size, colors::white, false);
}
