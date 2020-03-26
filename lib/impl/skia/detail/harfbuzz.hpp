/*=============================================================================
   Copyright (c) 2016-2020 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#include <hb.h>
#include <hb-ot.h>
#include <SkStream.h>
#include <SkTypeface.h>

namespace cycfi::artist::detail
{
   struct hb_blob
   {
                  hb_blob(std::unique_ptr<SkStreamAsset> asset);
                  hb_blob(hb_blob const&) = delete;
                  ~hb_blob()  { hb_blob_destroy(blob); }
      hb_blob_t*  get()       { return blob; }

   private:

      hb_blob_t* blob = nullptr;
   };

   struct hb_font
   {
                  hb_font(SkTypeface* tf);
                  hb_font(hb_font const&) = delete;
                  ~hb_font()  { hb_font_destroy(font); }
      hb_font_t*  get()       { return font; }

   private:

      hb_font_t* font = nullptr;
   };
}

