# Artist Cairo HarfBuzz Text Plan

## Goal

Add HarfBuzz shaping to the Cairo backend text path in staged, low-risk steps.

The Cairo backend already has:

- Fontconfig font selection
- FreeType-backed Cairo fonts via `cairo-ft`
- Cairo canvas text rendering
- Cairo `text_layout` with cluster-aware glyph drawing
- passing visual tests

The goal is to improve text shaping while preserving the current working behavior.

## Scope

Keep the first pass simple:

- primary selected font only
- left-to-right text only
- no font fallback
- no bidirectional text
- no public API changes
- no Skia refactor
- no large text-layout rewrite in the first stage (except HB5, which is necessarily structural)

Use the current Skia implementation as a reference where useful, but do not blindly copy it.
The Cairo backend uses different font plumbing (FreeType via `cairo-ft`, not `SkTypeface`), so
the HarfBuzz creation path differs.

## Public API to preserve

Do not change these APIs:

```cpp
font::metrics() const;
font::line_height() const;
font::measure_text(std::string_view) const;

canvas::font(font const&);
canvas::measure_text(std::string_view);
canvas::fill_text(std::string_view, point);
canvas::stroke_text(std::string_view, point);
canvas::text_align(...);
canvas::text_baseline(...);

text_layout::text(...);
text_layout::flow(...);
text_layout::draw(...);
text_layout::caret_point(...);
text_layout::caret_index(...);
```

## HarfBuzz font creation: Cairo vs Skia

This is the critical difference between the two backends.

**Skia path** (`lib/impl/skia/detail/harfbuzz.cpp`):

```text
SkTypeface → openStream() → hb_blob_create → hb_face_create → hb_font_create → hb_ot_font_set_funcs
```

**Cairo path** (to implement):

```text
cairo_scaled_font_t → cairo_ft_scaled_font_lock_face → FT_Face
                    → hb_ft_font_create_referenced(FT_Face)
                    → cairo_ft_scaled_font_unlock_face
```

Use `hb_ft_font_create_referenced` (from `<hb-ft.h>`).
The FT_Face is obtained once at font construction via `cairo_ft_scaled_font_lock_face`.
`cairo-ft` already links FreeType, so this requires no new shared library — only a new pkg-config header.

Do NOT use the blob-stream approach from Skia. That requires `SkTypeface` and `SkStreamAsset`.

## HarfBuzz position units

`hb_ft_font_create_referenced` sets the HarfBuzz font scale automatically to match the
FreeType face's current size. Positions from `hb_glyph_position_t` are then in FreeType's
26.6 fixed-point format (1/64 pixel units). Divide by 64.0 to get user-space pixel values.

Alternatively, call `hb_font_set_scale(hb_font, upem, upem)` after creation and then scale
by `(font_size / upem)` — the same approach Skia uses. This is the recommended approach as
it is cleaner and consistent with the Skia reference.

The Skia scale conversion:

```cpp
int hb_scalex, hb_scaley;
hb_font_get_scale(_hb_font.get(), &hb_scalex, &hb_scaley);
float scalex = (sc_font->getSize() / hb_scalex) * sc_font->getScaleX();
```

Use the same pattern for Cairo, replacing `sc_font->getSize()` with `descr._size` from
the font descriptor, and omitting `getScaleX()` (Cairo uses an identity CTM).

## Metrics policy

Use HarfBuzz for shaped horizontal advance.

Use Cairo/FreeType scaled-font metrics for vertical metrics.

Expected behavior:

```text
text width  = HarfBuzz shaped advance (sum of x_advance * scale)
text height = ascent + descent
```

Do not use visual glyph bounds as the main text height.

`font::metrics()` should remain font-wide metrics (unchanged, from `cairo_scaled_font_extents`).

`canvas::measure_text()` should report:

- ascent, descent, leading from `cairo_scaled_font_extents`
- size.x from shaped HarfBuzz advance
- size.y from ascent + descent

## HB5 data model change

This is the most important structural note in the plan.

**Current Cairo `text_layout` model** (code-point-centric):

```cpp
struct char_info { double x_advance; break_enum line_brk; break_enum word_brk; };
struct row_info  { size_t start; size_t end; float y; float width;
                   std::vector<float> positions; };  // positions indexed by UTF-32 offset
```

Flow iterates over UTF-32 code points. Drawing rebuilds glyph arrays from Cairo per line.

**Target model after HB5** (glyph-centric, mirrors Skia):

```cpp
// break info stays on code points (libunibreak still uses UTF-32)
struct break_info { break_enum line; break_enum word; };

// shaped glyph (from HarfBuzz, analogous to Skia's glyph_info + position)
// stored once at construction, reused across flow() calls
struct shaped_glyph {
   uint32_t  codepoint;    // HarfBuzz glyph ID (not Unicode codepoint)
   uint32_t  cluster;      // UTF-32 source index (for caret mapping)
   float     x_advance;    // scaled user-space advance
   float     x_offset;     // scaled user-space offset
   float     y_offset;
};

struct row_info {
   size_t             glyph_start;  // index into _glyphs
   size_t             glyph_count;
   float              y;
   float              width;
   std::vector<float> positions;    // x per glyph (after justify)
};
```

`caret_point(index)`: walk `_glyphs` to find the first glyph whose `cluster == index`,
look up its row and position. (Mirrors Skia's `glyphs_info.glyph_index(index)`.)

`caret_index(point)`: find row by y, binary-search `positions`, return
`_glyphs[row.glyph_start + pos].cluster`. (Mirrors Skia's `caret_index`.)

This is a structural rewrite of `text_layout::impl`, not a patch. Plan accordingly.
The `_chars` array is replaced by `_glyphs` (shaped once) plus `_breaks` (from libunibreak).
Flow iterates over glyphs, using `_breaks[glyph.cluster]` to look up break opportunities —
exactly as Skia does with `_breaks[glyphs_info.glyphs[glyph_idx].cluster]`.

---

## Stage HB0: Reconnaissance

### Goal

Understand the current Skia HarfBuzz implementation and current Cairo text path.

### Already known (do not re-derive)

From reading the source before writing this plan:

- Skia wraps HarfBuzz in `lib/impl/skia/detail/harfbuzz.hpp/.cpp` using `non_copyable` RAII
  wrappers with `deleter<T, destroy_fn>` unique_ptr aliases.
- Skia creates `hb_font_t` from `SkTypeface` via a blob stream, then calls
  `hb_ot_font_set_funcs` and optionally `hb_font_set_variations`.
- Skia shapes the entire text once in `impl` constructor, then reuses shaped glyphs across
  `flow()` calls.
- Cairo already calls `cairo_scaled_font_text_to_glyphs` with cluster info in both
  `build_char_advances()` and `draw()`. It already handles ignorable code points and
  justified glyph positions. It is more sophisticated than a naive `cairo_show_text` path.
- Cairo `font_impl` currently holds only `cairo_scaled_font_t*`.
- Cairo `text_layout::impl` currently holds `font`, `std::u32string`, `std::string` (UTF-8),
  `std::vector<char_info>`, and `std::vector<row_info>`.

### Tasks

Confirm and document:

1. Exact `deleter<>` template used in `infra/support.hpp` (needed for Cairo HB wrappers).
2. Whether `cairo_ft_scaled_font_lock_face` is available in the project's Cairo headers.
3. Whether `hb-ft.h` is installed alongside `hb.h` in the current build environment.
4. Whether `harfbuzz` pkg-config module includes `hb-ft.h` or requires `harfbuzz-ft`.
5. What `font_descr._size` represents (points or pixels) — needed for the scale calculation.
6. What the current Cairo canvas `fill_text` / `stroke_text` use: confirm it is
   `cairo_show_text` (not `cairo_show_glyphs`).

### Deliverable

Write:

```text
ai/harfbuzz-cairo-handoff.md
```

With:

```md
# Cairo HarfBuzz reconnaissance

## Current Skia HarfBuzz path

## Current Cairo text path

## Current Cairo font ownership

## HarfBuzz font creation: confirmed approach

## infra/support.hpp: deleter template

## pkg-config / header availability

## Current tests

## Risks

## Recommended next stage
```

### Stop condition

No functional code changes.

---

## Stage HB1: Add HarfBuzz dependency and private font plumbing

### Goal

Add HarfBuzz to the Cairo backend build and extend Cairo private font state with
a HarfBuzz font object.

### Scope

Cairo backend only. Do not change text rendering behavior yet.

### HarfBuzz font creation approach

```cpp
// In font.cpp, during make_scaled_font or font_impl construction:
FT_Face ft_face = cairo_ft_scaled_font_lock_face(scaled_font);
hb_font_t* hb = hb_ft_font_create_referenced(ft_face);
cairo_ft_scaled_font_unlock_face(scaled_font);

// Set scale to upem so positions are in font units, then scale by font_size/upem:
hb_font_set_scale(hb, hb_face_get_upem(hb_font_get_face(hb)),
                      hb_face_get_upem(hb_font_get_face(hb)));
```

Store the resulting `hb_font_t*` in `font_impl` with a custom deleter.

Required new header: `<hb-ft.h>`.

### font_impl changes

```cpp
struct font_impl
{
   cairo_scaled_font_t* _scaled_font = nullptr;
   hb_font_t*           _hb_font     = nullptr;   // NEW

   // destructor: hb_font_destroy(_hb_font) before cairo_scaled_font_destroy
};
```

Use the same RAII pattern as Skia (`deleter<hb_font_t, hb_font_destroy>`) if
`infra/support.hpp` provides `deleter<>`. Otherwise use a plain custom deleter.

### CMakeLists.txt changes

Add HarfBuzz only when `ARTIST_CAIRO=ON`:

```cmake
if(ARTIST_CAIRO)
   pkg_check_modules(HARFBUZZ REQUIRED harfbuzz)
   # or: pkg_check_modules(HARFBUZZ REQUIRED harfbuzz harfbuzz-ft)
   # depending on what HB0 finds
   target_link_libraries(artist PRIVATE ${HARFBUZZ_LIBRARIES})
   target_include_directories(artist PRIVATE ${HARFBUZZ_INCLUDE_DIRS})
endif()
```

### Success criteria

- Cairo builds.
- Existing Cairo tests pass.
- Quartz2D tests still pass.
- Skia tests still pass if run.
- No visual goldens change.
- No public API changes.
- `font_impl` holds a valid `hb_font_t*` that is properly destroyed.

### Handoff

Update `ai/harfbuzz-cairo-handoff.md`:

```md
## Current stage

HB1: HarfBuzz dependency and font plumbing

## What changed

## Files touched

## HarfBuzz dependency (pkg-config module used)

## font_impl ownership model

## Scale configuration (upem-based or 26.6)

## Build commands tried

## Test commands tried

## Test results

## Recommended next stage
```

---

## Stage HB2: Add a private Cairo shape_text helper

### Goal

Add a private helper that shapes one UTF-8 LTR run using HarfBuzz and returns
scaled glyph positions. Do not wire it into rendering yet.

### Scope

Private Cairo backend only. New file or a `detail/harfbuzz.hpp` analogous to Skia's,
but simpler — no `hb_blob` / stream path needed.

### Suggested file

```text
lib/impl/cairo/detail/harfbuzz.hpp   (optional, or inline in cairo_private.hpp)
```

Keep it minimal. Cairo does not need the blob/stream creation path.

### Shaped data model

Use project style:

```cpp
struct shaped_glyph
{
   uint32_t    codepoint;  // HarfBuzz glyph ID
   uint32_t    cluster;    // UTF-32 source index
   float       x_advance;  // user-space pixels
   float       x_offset;
   float       y_offset;
};

struct shaped_run
{
   std::vector<shaped_glyph> glyphs;
   float                     advance_x;
};
```

### Scale conversion

After `hb_shape`, positions from `hb_glyph_position_t` are in HarfBuzz font units
(if HB1 used the upem-scale approach). Convert to user-space pixels:

```cpp
int hb_scale;
hb_font_get_scale(hb_font, &hb_scale, nullptr);
float scale = font_size / float(hb_scale);

// Per glyph:
glyph.x_advance = pos.x_advance * scale;
glyph.x_offset  = pos.x_offset  * scale;
glyph.y_offset  = pos.y_offset  * scale;
```

This mirrors exactly what Skia does in `text_layout.cpp`:

```cpp
float scalex = (sc_font->getSize() / hb_scalex) * sc_font->getScaleX();
```

### Tasks

1. Create the `shape_text` helper (free function or small struct).
2. Accept `hb_font_t*`, `font_size` (float), and UTF-32 text.
3. Shape with `hb_shape`.
4. Convert positions using the scale formula above.
5. Preserve cluster values.
6. Zero-advance ignorable code points (match current Cairo behavior in
   `build_char_advances`; HarfBuzz handles most automatically but verify).
7. Do not wire into production drawing yet.

### Important details

Do not assume:

```text
one character = one glyph
one byte = one glyph
one codepoint = one glyph
```

Cluster values are essential for HB5 caret mapping and line-break lookup.

### Success criteria

- Cairo builds.
- Existing tests pass.
- Helper shapes simple Latin text correctly.
- Scale conversion is correct (verify a known string's total advance matches
  `cairo_scaled_font_text_extents` within a small tolerance).
- No public API changes.
- No golden updates.

### Handoff

Document:

```md
## Current stage

HB2: private shape_text helper

## Helper location and signature

## Shaped run representation

## Unit conversion (scale formula used)

## Cluster behavior

## Ignorable code point handling

## Build/test results

## Known limitations

## Recommended next stage
```

---

## Stage HB3: Use HarfBuzz for text measurement

### Goal

Use HarfBuzz shaped advance for Cairo text measurement.

### Scope

This stage affects:

```cpp
font::measure_text(std::string_view)      // in lib/impl/cairo/font.cpp
canvas::measure_text(std::string_view)    // in lib/impl/cairo/canvas.cpp
```

Do not change drawing yet. Do not change `text_layout` yet.

### Current state

`font::measure_text` calls `cairo_scaled_font_text_extents` — no shaping, no kerning.
`canvas::measure_text` also calls `cairo_scaled_font_text_extents`.

### Implementation

Both functions should call `shape_text` (from HB2) using `_ptr->_hb_font` and return
`shaped_run.advance_x` as the width. Vertical metrics remain from `cairo_scaled_font_extents`.

`font::measure_text` returns a single `float` (the width). The `shape_text` helper needs to
accept UTF-8 text (convert to UTF-32 internally, as HarfBuzz takes UTF-32).

### Metrics policy

Use:

```text
width = shaped HarfBuzz advance (shaped_run.advance_x)
height = ascent + descent     (from cairo_scaled_font_extents, unchanged)
```

### Tasks

1. Call `shape_text` in `font::measure_text` and return `shaped_run.advance_x`.
2. Call `shape_text` in `canvas::measure_text` for width; preserve vertical metrics.
3. Confirm `text_align` and `text_baseline` still receive correct metrics.
4. Run tests.

### Tests

```sh
cmake -S . -B build-cairo -DARTIST_CAIRO=ON
cmake --build build-cairo
ctest --test-dir build-cairo --output-on-failure
```

Also run Quartz2D and Skia tests if practical.

### Golden policy

If Cairo typography output changes because measurement changes layout, review the PNG
before updating any golden. Width changes for plain Latin text should be small (HarfBuzz
and FreeType advances typically agree for LTR Latin without ligatures).

### Success criteria

- Cairo measurement uses HarfBuzz shaped advances.
- Vertical metrics remain stable.
- Cairo tests pass, or intentional golden updates are reviewed.
- Non-Cairo backends are unaffected.

### Handoff

Document:

```md
## Current stage

HB3: HarfBuzz measurement

## Measurement behavior

## Metrics behavior

## Test results

## Goldens updated, if any

## Known limitations

## Recommended next stage
```

---

## Stage HB4: Use HarfBuzz for canvas fill_text and stroke_text

### Goal

Use HarfBuzz-shaped glyphs for Cairo canvas text drawing.

### Scope

This stage affects:

```cpp
canvas::fill_text(std::string_view, point)    // in lib/impl/cairo/canvas.cpp
canvas::stroke_text(std::string_view, point)  // in lib/impl/cairo/canvas.cpp
```

Do not rewrite `text_layout` yet.

### Current state

`canvas::fill_text` calls `cairo_show_text` — no shaping.
`canvas::stroke_text` uses `cairo_text_path` then `cairo_stroke` — no shaping.

### Implementation

1. Call `shape_text` (from HB2).
2. Convert each `shaped_glyph` to a `cairo_glyph_t`:

```cpp
cairo_glyph_t g;
g.index = shaped.codepoint;   // HarfBuzz glyph ID == Cairo/FreeType glyph index
g.x = pen_x + shaped.x_offset + alignment_offset;
g.y = pen_y + shaped.y_offset + baseline_offset;
pen_x += shaped.x_advance;
```

3. For `fill_text`: call `cairo_show_glyphs(ctx, glyphs.data(), int(glyphs.size()))`.
4. For `stroke_text`: call `cairo_glyph_path(ctx, glyphs.data(), int(glyphs.size()))`,
   then apply stroke.

### Positioning policy

```text
pen position
+ HarfBuzz x_offset / y_offset (from shaped_glyph)
+ alignment offset  (from text_align, same as current)
+ baseline offset   (from text_baseline, same as current)
```

Alignment and baseline logic is unchanged — it already computes an offset from
`measure_text` (which after HB3 returns shaped advance). No further changes needed there.

### Success criteria

- Cairo `fill_text` uses HarfBuzz-shaped glyphs.
- Cairo `stroke_text` uses HarfBuzz-shaped glyph paths.
- Cairo tests pass.
- Any changed Cairo typography golden is reviewed.
- Non-Cairo backends are unaffected.

### Handoff

Document:

```md
## Current stage

HB4: HarfBuzz canvas drawing

## Drawing behavior

## Alignment behavior

## Baseline behavior

## Test results

## Goldens updated, if any

## Known limitations

## Recommended next stage
```

---

## Stage HB5: Integrate HarfBuzz into Cairo text_layout

### Goal

Use HarfBuzz-shaped runs inside Cairo `text_layout`.

This is the main structural stage. The data model changes significantly.

### Scope

Affects `lib/impl/cairo/text_layout.cpp` only:

```cpp
text_layout::flow(...)
text_layout::draw(...)
text_layout::caret_point(...)
text_layout::caret_index(...)
```

Still keep it simple:

- primary font only
- LTR only
- no fallback
- no bidi

### Data model migration

Replace the code-point-centric model with the glyph-centric model described in the
"HB5 data model change" section above.

Shape the entire text once in `impl` constructor (as Skia does).
Store `_glyphs` (shaped) and `_breaks` (from libunibreak on UTF-32).

Flow iterates over `_glyphs`, using `_breaks[glyph.cluster]` for break decisions —
exactly as Skia uses `_breaks[glyphs_info.glyphs[glyph_idx].cluster]`.

`row_info` stores glyph indices, not code-point indices.

`draw()` converts `shaped_glyph.codepoint` + per-glyph position to `cairo_glyph_t`
and calls `cairo_show_glyphs` — no per-line `cairo_scaled_font_text_to_glyphs` call needed.

### Keep libunibreak

Continue using libunibreak for line and word break opportunities.
HarfBuzz shapes text. It does not replace line breaking.
libunibreak still operates on UTF-32 code points.

### Required behavior to preserve

- wrapping
- justification (spread spaces when line ≥90% full, on space glyphs)
- caret point
- caret index
- break character behavior
- source text preservation
- line count

### Cluster policy

Use HarfBuzz cluster values (`glyph.cluster`) for:

1. Break lookup: `_breaks[glyph.cluster].line`
2. `caret_point`: find first glyph with `cluster == index`, return its x position.
3. `caret_index`: find glyph at x position, return `glyph.cluster`.

Do not split inside shaped clusters (e.g. ligatures).
Conservative caret behavior around ligatures is acceptable for the first pass.

### Justification

Apply justification to space glyphs only: `is_space(_text[glyph.cluster])`.
Matches current behavior.

### Tests

Run all existing tests. Add focused tests only if needed for:

- ligature clusters
- multi-byte UTF-8
- caret around shaped clusters
- justification after shaping

Avoid fragile font-dependent assertions unless the font is stable.

### Golden policy

Typography output may change. Review before updating Cairo goldens.
Differences between old (FreeType-only) and new (HarfBuzz-shaped) output are expected
for text with kerning or ligatures.

### Success criteria

- Cairo `text_layout` uses HarfBuzz-shaped glyphs.
- Existing wrapping behavior remains correct.
- Existing caret tests pass.
- Justification still works.
- Cairo tests pass.
- `cairo_scaled_font_text_to_glyphs` is no longer called from `text_layout`.
- Limitations are documented.

### Handoff

Document:

```md
## Current stage

HB5: HarfBuzz text_layout

## Layout behavior

## Cluster mapping

## Caret behavior

## Justification behavior

## Test results

## Goldens updated, if any

## Known limitations

## Recommended next stage
```

---

## Stage HB6: Stabilize shaping tests

### Goal

Make HarfBuzz-related tests reliable across machines.

### Problem

Fontconfig may choose different fonts on different platforms or systems.
Shaping behavior, metrics, and ligatures may differ depending on installed fonts.

### Options

1. Keep tests metric-light.
2. Use platform-specific goldens.
3. Install a known font in CI.
4. Add a small open-license test font, only if licensing is clear.

### Tasks

1. Audit any HarfBuzz-specific tests added in HB3–HB5.
2. Identify font assumptions.
3. Decide whether current CI fonts are stable enough.
4. Add explicit CI font dependency only if needed.
5. Avoid committing font files unless explicitly approved.

### Success criteria

- Tests are stable.
- Font assumptions are documented.
- CI behavior is predictable.

---

## Stage HB7: Document deferred features

### Goal

Document what HarfBuzz now supports and what remains deferred.

### Deferred features

Keep these out of the first HarfBuzz pass:

- font fallback
- bidirectional text
- script run segmentation
- explicit OpenType feature controls
- advanced font fallback policy
- complex cross-font caret mapping

### Documentation points

Document:

```text
Cairo uses HarfBuzz for shaping primary-font LTR text.
Fontconfig still selects the font.
FreeType still loads the font (via cairo-ft).
libunibreak still handles line breaks.
HarfBuzz font created via hb_ft_font_create_referenced.
No fallback fonts yet.
No bidi yet.
```

### Success criteria

- README or backend docs are updated.
- Handoff is updated.
- Limitations are clear.

---

# Suggested Claude Code task sequence

## Task 1

```text
Do HB0 only: audit the current Skia HarfBuzz implementation and current Cairo text path.
Confirm: (1) the deleter<> template in infra/support.hpp, (2) whether cairo_ft_scaled_font_lock_face
is available, (3) whether hb-ft.h is installed or requires a separate pkg-config module,
(4) what font_descr._size represents (points vs pixels). Do not change code.
Write findings to ai/harfbuzz-cairo-handoff.md.
```

## Task 2

```text
Do HB1 only: add HarfBuzz dependency and private Cairo font plumbing.
Use hb_ft_font_create_referenced (from <hb-ft.h>) with cairo_ft_scaled_font_lock_face.
Set HB font scale to upem x upem. Store hb_font_t* in font_impl with correct destruction.
Do not change rendering behavior yet.
```

## Task 3

```text
Do HB2 only: add a private shape_text helper for one UTF-8 LTR run.
Accept hb_font_t* and font_size. Shape with hb_shape. Convert positions using
(font_size / hb_scale) as in the plan. Return shaped_glyph vector with cluster values.
Do not wire into rendering yet.
```

## Task 4

```text
Do HB3 only: use HarfBuzz shaped advances for Cairo font::measure_text and
canvas::measure_text. Preserve vertical metrics from cairo_scaled_font_extents.
```

## Task 5

```text
Do HB4 only: use HarfBuzz shaped glyphs for Cairo canvas::fill_text and
canvas::stroke_text. Replace cairo_show_text with cairo_show_glyphs using
shaped glyph positions.
```

## Task 6

```text
Do HB5 only: integrate HarfBuzz shaped runs into Cairo text_layout.
Migrate from the code-point-centric model (_chars / row {start,end}) to the
glyph-centric model (_glyphs / row {glyph_start, glyph_count}) as described in the plan.
Preserve wrapping, justification, and caret behavior. Draw with cairo_show_glyphs.
```

## Task 7

```text
Do HB6 only: stabilize HarfBuzz-related tests and document font assumptions.
```

## Task 8

```text
Do HB7 only: update documentation for Cairo HarfBuzz support and deferred features.
```

---

# Review checklist for every stage

Before stopping:

```md
- [ ] Diff is focused.
- [ ] Cairo tests pass.
- [ ] Quartz2D tests pass if practical.
- [ ] Skia tests pass if practical.
- [ ] No public API changes unless explicitly justified.
- [ ] No broad formatting-only changes.
- [ ] HarfBuzz objects are destroyed correctly.
- [ ] FreeType/Fontconfig ownership remains correct.
- [ ] Vertical metrics semantics are preserved.
- [ ] Goldens are updated only after review.
- [ ] ai/harfbuzz-cairo-handoff.md is updated.
```

# Definition of done

The first HarfBuzz pass is complete when:

```md
- [ ] Cairo links HarfBuzz only when ARTIST_CAIRO is enabled.
- [ ] Cairo font state owns hb_font_t* safely (via hb_ft_font_create_referenced).
- [ ] Cairo text measurement uses shaped advances.
- [ ] Cairo fill_text uses shaped glyphs (cairo_show_glyphs).
- [ ] Cairo stroke_text uses shaped glyph paths (cairo_glyph_path).
- [ ] Cairo text_layout uses shaped runs (glyph-centric model).
- [ ] Wrapping still uses libunibreak.
- [ ] Justification still works.
- [ ] Caret mapping is cluster-aware enough for current tests.
- [ ] Existing tests pass.
- [ ] Limitations documented: primary font only, LTR only, no fallback, no bidi.
- [ ] cairo_scaled_font_text_to_glyphs removed from text_layout (replaced by HarfBuzz).
```
