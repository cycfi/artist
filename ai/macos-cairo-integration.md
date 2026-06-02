# macOS Cairo Integration — Drawing, Context, and Font Support

This document explains the macOS-specific design decisions in the Cairo backend,
covering the drawing surface, coordinate system, font architecture, and the
surface-type dispatch that makes everything work together.

---

## Drawing Surface

### Why `cairo_quartz_surface_create_for_cg_context`

The macOS host (`examples/host/macos/cairo_app.mm`) uses:

```objc
auto surface = cairo_quartz_surface_create_for_cg_context(
   cg_ctx, bounds.size.width, bounds.size.height);
auto cairo_ctx = cairo_create(surface);
```

This wraps AppKit's existing `CGContext` directly. Cairo draws into the
window's backing store without any intermediate pixel buffer. The alternative
(the old approach) was `cairo_image_surface_create` + `CGBitmapContextCreate`
+ `CGContextDrawImage` — a full pixel-buffer copy every frame.

**Do not switch back to the image surface approach.** It breaks CG-backed font
rendering (see Font section) and is significantly slower.

### Coordinate System (`isFlipped=YES`)

The `CocoaView` overrides `isFlipped` to return `YES`:

```objc
-(BOOL) isFlipped { return YES; }
```

This makes AppKit supply a top-down CGContext (origin at top-left, y increases
downward). `cairo_quartz_surface_create_for_cg_context` wraps the context
**without adding its own flip**, so Cairo's coordinate system is also top-down.
Artist drawing code uses top-down coordinates throughout, so this is correct.

**Do not remove `isFlipped=YES`.** Without it the CGContext is bottom-up and all
drawing appears vertically mirrored.

### Retina / HiDPI

The CGContext supplied by AppKit already encodes the device scale (2× on
Retina). `bounds.size.width/height` are in points (logical pixels). Cairo
works in points; the display system composites to physical pixels automatically.
No explicit `cairo_surface_set_device_scale` is needed.

---

## Font Architecture

Fonts need to work in two rendering contexts:

| Context | Surface type | What renders text |
|---------|-------------|-------------------|
| Live canvas (window) | `CAIRO_SURFACE_TYPE_QUARTZ` | CG-backed font face |
| Tests / offscreen | `CAIRO_SURFACE_TYPE_IMAGE` or recording | FT-backed scaled font |

### `font_impl` — dual-face structure

```cpp
struct font_impl
{
   cairo_font_face_t*   _face;        // CG face — Quartz only
   cairo_scaled_font_t* _scaled_font; // FT scaled font — metrics + non-Quartz rendering
   float                _size;
   hb_font_ptr          _hb_font;     // HarfBuzz — always FT-backed
};
```

**`_face` (CG-backed):**
Created via `CGFontCreateWithFontName(FC_FULLNAME)` →
`cairo_quartz_font_face_create_for_cgfont`. Works correctly under the
`isFlipped=YES` CTM. Used only when the target surface is Quartz.

`FC_FULLNAME` from fontconfig is the name `CGFontCreateWithFontName` accepts
for system-installed fonts. For bundled fonts, `CTFontManagerRegisterFontsForURL`
must be called first (done in `init_paths()` in `cairo_app.mm`).

**`_scaled_font` (FT-backed):**
Created via `cairo_ft_font_face_create_for_pattern` +
`cairo_scaled_font_create` with `HINT_METRICS_OFF`. Serves two purposes:
1. Standalone `font::metrics()` and `font::measure_text()` — called without a
   canvas context, so a pre-built scaled font is required.
2. `cairo_set_scaled_font` on image/recording surfaces (tests, offscreen
   rendering) where CG faces fail silently.

**`_hb_font` (HarfBuzz):**
Bootstrapped from the FT face via `cairo_ft_scaled_font_lock_face` →
`hb_ft_face_create_referenced` → `hb_ot_font_set_funcs`. Locked briefly to
extract the FT_Face; HarfBuzz copies the font blob internally so the FT_Face
can be released after. Scale is set to `(upem, upem)` so HarfBuzz positions
are in font units, scaled to pixels by `size / upem` at shape time.

### Why FreeType Faces Fail on Quartz

FreeType font faces rasterise glyphs using FreeType's CPU renderer. On a
Quartz surface with the `isFlipped=YES` CTM (which has a negative Y scale),
FreeType glyph bitmaps are not correctly composited — they render invisibly.
CG-backed faces delegate rasterisation to Core Graphics, which handles the
flipped CTM correctly.

This failure is silent: `cairo_show_glyphs` returns no error but produces no
output. The only symptom is a blank window.

### Font Face Cache

`get_face_cache()` in `font.cpp` maintains a `std::map<std::string, face_entry>`
keyed by `FC_FULLNAME`:

```cpp
struct face_entry
{
   cairo_font_face_t* cairo_face; // CG on macOS, FT on other platforms
   hb_face_t*         hb_face;   // HarfBuzz face (immutable, safe to share)
};
```

On a cache hit, `cairo_font_face_reference` and `hb_face_reference` are used
to hand out additional references. `hb_font_t` is NOT cached — it is created
cheaply per `font_impl` from the shared `hb_face_t` (`hb_font_create` is O(1)).

The cache eliminates the expensive work on repeated `font(font_descr)` calls:
fontconfig pattern matching, `CGFontCreateWithFontName`, FreeType face loading,
and `hb_ft_face_create_referenced` all happen only once per unique typeface.

---

## Surface-Type Dispatch

Two call sites check the surface type at runtime:

### `canvas::font()` — `lib/impl/cairo/canvas.cpp`

```cpp
#ifdef __APPLE__
if (fi->_face &&
    cairo_surface_get_type(cairo_get_target(_context)) == CAIRO_SURFACE_TYPE_QUARTZ)
{
    cairo_set_font_face(_context, fi->_face);   // CG face
    cairo_set_font_size(_context, fi->_size);
}
else if (fi->_scaled_font)
{
    cairo_set_scaled_font(_context, fi->_scaled_font);  // FT scaled font
}
```

### `text_layout::impl::draw()` — `lib/impl/cairo/text_layout.cpp`

Identical dispatch, applied before `cairo_show_glyphs`.

The dispatch ensures:
- Quartz (live rendering): CG face → Core Graphics rasterisation → correct.
- Image/recording (tests, offscreen): FT scaled font → FreeType rasterisation
  with baked `HINT_METRICS_OFF` → consistent metrics across platforms.

---

## `cairo_show_glyphs` on Quartz Requires `cairo_move_to`

On the Quartz CG backend, `cairo_show_glyphs` silently produces no output
unless the Cairo current point is set beforehand. This is called before every
`cairo_show_glyphs` call in both `canvas::fill_text` and
`text_layout::impl::draw`:

```cpp
cairo_move_to(ctx, first_glyph.x, first_glyph.y);
cairo_show_glyphs(ctx, glyphs.data(), count);
```

Glyph positions in `cairo_glyph_t` are absolute user-space coordinates, so
the current point does not affect placement — it only satisfies the Quartz
backend's precondition.

---

## Font Registration for Bundled Fonts

System fonts (in `~/Library/Fonts`, `/Library/Fonts`, etc.) are automatically
known to Core Text and found by `CGFontCreateWithFontName`. Bundled fonts
inside the app bundle must be registered explicitly:

```objc
// In init_paths() — cairo_app.mm
for (fs::directory_iterator it{resource_path}; it != fs::directory_iterator{}; ++it)
    if (it->path().extension() == ".ttf")
        activate_font(it->path());

// activate_font:
void activate_font(cycfi::fs::path font_path)
{
    auto furl = [NSURL fileURLWithPath:[NSString stringWithUTF8String:font_path.c_str()]];
    CFErrorRef error = nullptr;
    CTFontManagerRegisterFontsForURL((__bridge CFURLRef)furl,
        kCTFontManagerScopeProcess, &error);
    if (error) CFRelease(error);
}
```

`init_paths()` is called lazily on first resource lookup via the `resource_setter`
in `resources.cpp`. It is guaranteed to run before any font is constructed.

Fontconfig (`fc_state` in `font.cpp`) independently adds the same resource
directory via `FcConfigAppFontAddDir` — this makes the fonts visible to
fontconfig for `FC_FULLNAME` / `FC_FILE` queries independently of Core Text.

---

## Summary of macOS-Specific Files

| File | Role |
|------|------|
| `examples/host/macos/cairo_app.mm` | Quartz surface creation, font registration, coordinate system |
| `lib/impl/cairo/font.cpp` | Dual-face creation, face cache, HarfBuzz bootstrap |
| `lib/impl/cairo/cairo_private.hpp` | `font_impl` struct with `_face` + `_scaled_font` + `_hb_font` |
| `lib/impl/cairo/canvas.cpp` | Surface-type dispatch in `canvas::font()`; `cairo_move_to` fix |
| `lib/impl/cairo/text_layout.cpp` | Surface-type dispatch in `draw()`; `cairo_move_to` fix |
| `lib/CMakeLists.txt` | Links `CoreGraphics` and `CoreFoundation` for Cairo+Apple |
