/*=============================================================================
   Copyright (c) 2016-2026 Joel de Guzman

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#if !defined(ARTIST_RECORDING_SEPTEMBER_13_2026)
#define ARTIST_RECORDING_SEPTEMBER_13_2026

#include <artist/canvas.hpp>
#include <string>
#include <vector>

namespace cycfi::artist::recording
{
   ////////////////////////////////////////////////////////////////////////////
   // The recording backend rasterizes nothing. It keeps a journal of what was
   // asked of the canvas, so a test can assert on the drawing itself rather
   // than on pixels: which operations ran, where, and in what paint.
   //
   // Coordinates are in surface space: the space of the canvas as it was
   // handed out, before any transform the drawing applies. For an image, that
   // is its size in units.
   //
   // Text is measured and shaped with real fonts (fontconfig, FreeType and
   // HarfBuzz), so layout matches what a drawing backend would produce.
   ////////////////////////////////////////////////////////////////////////////
   enum class op
   {
      fill,
      stroke,
      clear,
      fill_text,
      stroke_text,
      image
   };

   struct command
   {
      op                         kind;
      rect                       geometry;      // the extent of the shape itself
      rect                       bounds;        // geometry and shadow within the clip: the ink
      bool                       visible;       // bounds is not empty
      color                      paint;         // the colour, or a gradient's first stop
      bool                       gradient;
      float                      line_width;    // the stroke state in effect
      canvas::line_cap_enum      line_cap;
      canvas::join_enum          line_join;
      float                      miter_limit;
      point                      shadow_offset; // the shadow state in effect
      float                      shadow_blur;
      color                      shadow_color;
      canvas::composite_op_enum  composite;
      std::string                text;          // text operations only
   };

   class journal
   {
   public:

      std::vector<command> const&   commands() const   { return _commands; }
      std::size_t                   size() const       { return _commands.size(); }
      std::size_t                   count(op kind) const;

      // The union of the ink of every visible operation, or an empty rect
      // when nothing landed.
      rect                          ink_extents() const;

      // Operations issued but wholly outside the clip: work that reached the
      // canvas and could have been culled before it.
      std::size_t                   clipped_away() const;

      // One line per operation, for text goldens.
      std::string                   str() const;
      void                          clear()            { _commands.clear(); }

      void                          add(command const& cmd);

   private:

      std::vector<command>          _commands;
   };

   // The journal an offscreen_image over `img` records into.
   journal&                         journal_of(image& img);
   journal const&                   journal_of(image const& img);
}

#endif
