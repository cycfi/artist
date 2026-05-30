# Artist Cairo backend handoff

## Current stage

Stage 3: Existing tests plus minimal Cairo smoke coverage — complete.

---

## Existing tests inspected

`test/main.cpp` — single Catch2 binary with 8 test cases:

| Test case   | What it covers                                                     |
|-------------|--------------------------------------------------------------------|
| Drawing     | basics, transforms, balloon/heart beziers, pixmap, line styles, gradients |
| Drawing2    | round-rect shapes                                                  |
| Typography  | fill/stroke text, font metrics, text_layout flow/caret/glyph       |
| Composite   | offscreen_image compositing, all blend modes                       |
| DropShadow  | shadow_style (no-op in Cairo)                                      |
| Paths       | SVG path parsing, arc, bezier, quadratic                           |
| Misc        | clear_rect, transforms, skew, path clip, point_in_path, measure_text |
| Color Maths | arithmetic on `color` — pure math, no canvas                       |

All visual tests use `compare_golden` (SSI threshold 0.985) against pre-generated golden PNGs.

---

## Default backend test result

**Quartz2D** (`cmake -S . -B build-quartz -DARTIST_QUARTZ_2D=ON`): **PASS** — 8/8 test cases, 0 failures.

---

## Cairo backend test result

**Cairo** (`cmake -S . -B build-cairo -DARTIST_CAIRO=ON`): **6/8 test cases pass**.

SSI scores against Stage 3 golden images:

```
SSI result for shapes_and_images : 1
SSI result for shapes2           : 1
SSI result for typography        : 1
SSI result for composite_ops     : 1
SSI result for drop_shadow       : 1
SSI result for paths             : 1
SSI result for misc              : 1
```

Failing test cases (font/text stubs only — all Stage 5):

- **Typography**: 35+ CHECK failures — font metrics wrong, text_layout fully stubbed.
- **Misc**: 4 CHECK failures — `measure_text` returns toy-font values.

---

## Cairo-specific failures found and fixed

### 1. Crash: `cairo_surface_mark_dirty` on PNG-loaded surface

`cairo_image_surface_create_from_png` attaches MIME data to the surface.
Calling `cairo_surface_mark_dirty` on it triggers a Cairo assertion abort.

**Fix:** Only call `cairo_surface_mark_dirty` in the stb_image branch (after writing
pixels directly). Never call it on surfaces loaded via `cairo_image_surface_create_from_png`.

### 2. Crash: `text_layout::caret_index` stub returning `npos`

The Typography test accessed `text[npos]` causing a segfault.

**Fix:** Stub returns `0` instead of `npos`. CHECK assertions still fail (stub not
implemented), but no crash.

### 3. Crash: `text_layout::text()` stub returning empty view

Typography test line 622: `*(tlayout.text().data() + i)` — with `data()` null, segfault.

**Fix:** Stub now stores the text it is constructed with and returns it from `text()`.
Minimal ASCII UTF-8→UTF-32 conversion; multi-byte chars become `?`. No layout is done.

### 4. Missing Cairo golden path in `test/CMakeLists.txt`

No `ARTIST_CAIRO` branch in the `GOLDEN_PATH` block. All visual tests threw
"File does not exist" for every golden PNG.

**Fix:** Added `elseif (ARTIST_CAIRO) set(GOLDEN_PATH macos_golden/cairo)`.
Created `test/macos_golden/cairo/` and populated it from the first Cairo run.

### 5. Wrong rendering: `clip(path)` ignoring path fill rule

The Misc test clips to a donut (two concentric circles, `fill_odd_even`). Cairo
produced a solid disk — the inner hole was missing.

**Root cause:** `canvas::clip(path)` appended the path geometry but did not transfer
the path's `fill_rule` to the Cairo context before `cairo_clip`. Cairo clips using
the current context fill rule, so the odd-even rule was silently ignored.

**Fix:** Call `cairo_set_fill_rule` from `p.impl()->fill_rule` immediately before
`cairo_clip` inside `canvas::clip(path const&)`.

**Golden updated:** `misc.png` regenerated.

### 6. Silent data loss: Cairo unbounded operators clearing the whole surface

All six composite modes in the first two rows of the Composite test rendered blank.
Subsequent operators (destination_in, lighter, etc.) rendered correctly.

**Root cause:** Cairo's *unbounded* operators (`IN`, `OUT`, `SOURCE`, `DEST_IN`,
`DEST_ATOP`, `XOR`) apply to the **entire current clip region**, not just the filled
path. `canvas::draw(image)` used `cairo_rectangle` + `cairo_fill` with no active clip.
So every `draw` call with an unbounded operator zeroed the full 640×480 surface,
erasing all previously rendered cells. The bug was invisible per-cell (the cell itself
rendered correctly) but destructive globally.

The failure pattern was: cells drawn with bounded operators (destination_in, ADD,
DARKEN, SOURCE, XOR-over-transparent) appeared to survive because they were the last
writes, while the six cells drawn first (source_over, source_atop, source_in,
source_out, destination_over, destination_atop) were erased by later unbounded draws.

**Fix:** Wrap the image paint in a clip confined to the destination rectangle, then
use `cairo_paint` instead of `cairo_fill`:
```cpp
cairo_rectangle(_context, 0, 0, src.width(), src.height());
cairo_clip(_context);          // confine unbounded operators to dest rect
cairo_set_source_surface(...);
cairo_paint(_context);         // fill the whole clip region
```

**Golden updated:** `composite_ops.png` regenerated — all 12 blend modes now correct.

---

## Typography rendering: known wrong (Stage 5)

The Typography golden captures Cairo's current (incorrect) output as a regression
baseline. Comparing Cairo vs Quartz2D:

| Feature                    | Cairo (current)             | Quartz2D                         |
|----------------------------|-----------------------------|----------------------------------|
| Font size                  | Wrong — toy API ignores pt  | Correct                          |
| Weight/style (Bold, Light…)| Ignored — toy API only      | Correct via font_descr           |
| Gradient fill on text      | Works                       | Works                            |
| Outline stroke text        | Partial                     | Correct                          |
| shadow_style               | No-op                       | Drop shadow + glow               |
| text_layout paragraph      | Renders nothing             | Full layout via HarfBuzz         |
| Text alignment             | Lines drawn, text misplaced | Correct                          |

All typography failures are Stage 5 (font matching via FreeType/Fontconfig, real
metrics, text_layout implementation).

---

## What changed

### `lib/impl/cairo/canvas.cpp`
- `canvas::clip(path const&)`: added `cairo_set_fill_rule` from `p.impl()->fill_rule`
  before `cairo_clip` so odd-even paths clip correctly.
- `canvas::draw(image, src, dest)`: replaced `cairo_rectangle` + `cairo_fill` with
  `cairo_rectangle` + `cairo_clip` + `cairo_set_source_surface` + `cairo_paint`.
  Confines unbounded Cairo operators to the destination rect.

### `lib/impl/cairo/image.cpp`
- `image::image(fs::path)`: removed `cairo_surface_mark_dirty` from after the if/else
  block; call it only inside the stb_image branch (after writing pixels directly).

### `lib/impl/cairo/text_layout.cpp`
- `text_layout::impl` now holds `std::u32string _text`.
- Constructors and `text()` setters store the input text.
- `text() const` returns a view over the stored text.
- `caret_index(point)` returns `0` instead of `npos`.
- Added `#include <memory>`.

### `test/CMakeLists.txt`
- Added `elseif (ARTIST_CAIRO) set(GOLDEN_PATH macos_golden/cairo)`.

### `test/macos_golden/cairo/` (new)
- 7 PNG golden images for all visual test cases.
- `misc.png` and `composite_ops.png` were regenerated after bug fixes.
- `typography.png` captures the known-wrong toy-font rendering as a baseline.

---

## Files touched

- `lib/impl/cairo/canvas.cpp`
- `lib/impl/cairo/image.cpp`
- `lib/impl/cairo/text_layout.cpp`
- `test/CMakeLists.txt`
- `test/macos_golden/cairo/` (new directory + 7 PNG golden images)

---

## Build commands tried

```sh
cmake -S . -B build-quartz -DARTIST_QUARTZ_2D=ON -DARTIST_BUILD_TESTS=ON
cmake --build build-quartz --target artist_test

cmake -S . -B build-cairo -DARTIST_CAIRO=ON -DARTIST_BUILD_TESTS=ON
cmake --build build-cairo --target artist_test
```

---

## Test commands tried

```sh
ctest --test-dir build-quartz --output-on-failure
ctest --test-dir build-cairo  --output-on-failure
```

---

## Output artifact

Cairo-rendered PNGs in `build-cairo/test/results/`, promoted to
`test/macos_golden/cairo/` as Stage 3 golden images.

Visually inspected and compared against Quartz2D:
`shapes_and_images.png`, `paths.png`, `misc.png`, `composite_ops.png`, `typography.png`.

---

## Features exercised

Through existing test suite under Cairo:

- Solid color fill and stroke
- Rectangle, circle, round-rect
- Quadratic and bezier curves
- SVG path parsing (M, L, C, Q, A, Z)
- Arcs
- Line caps (butt, round, square) and joins (bevel, round, miter)
- Transforms: translate, scale, rotate, skew, save/restore
- `new_state` RAII guard
- `point_in_path` and `clip` (including odd-even fill rule)
- `fill_preserve` / `stroke_preserve`
- `add_path` with pre-built `path` objects
- `clear_rect`
- `offscreen_image` / offscreen compositing
- All 12 blend modes including unbounded Cairo operators
- `shadow_style` (no-op, no crash)
- PNG load and `draw(image)`
- Linear and radial gradients
- `fill_color` / `stroke_color`
- `affine_transform` roundtrip via `transform()` getter/setter
- Fill text / stroke text (toy font, incorrect metrics but no crash)

---

## Bugs fixed during testing

1. `cairo_surface_mark_dirty` on PNG-loaded surface → assertion abort → fixed.
2. `text_layout::caret_index` returning `npos` → segfault → fixed (returns 0).
3. `text_layout::text()` returning null view → segfault → fixed (stores text).
4. Missing Cairo golden path in CMake → all visual tests threw → fixed.
5. `clip(path)` ignoring path fill rule → donut rendered as solid disk → fixed.
6. Unbounded Cairo operators in `draw(image)` → entire surface cleared → fixed.

---

## Known limitations remaining

- **Typography**: toy font API — wrong metrics, wrong size, no font matching. Stage 5.
  - `measure_text`: wrong values (CHECK assertions fail).
  - `text_layout`: fully stubbed — no flow, no draw, no caret hit testing.
  - `shadow_style`: no-op. Stage 5 (render-to-surface + blur workaround needed).
  - `font::metrics()`: near-zero values. Stage 5.
- `path::operator==`: pointer identity only (not geometry equality).
- `image::bitmap_size()`: returns same as `size()` — no HiDPI factor. Stage 8.
- `make_image(data, fmt, size)`: throws. Stage 6.
- Cairo golden images for Linux and Windows do not exist yet.
- Stage 3 goldens are Cairo-vs-Cairo (not pixel-accurate vs Quartz2D). Stage 4
  will compare and close visual gaps.

---

## Recommended next task

```text
Do Stage 4 only: bring basic vector drawing behavior closer to Skia and Quartz2D.

Read ai/artist-cairo-backend-assimilation-plan.md Stage 4 section before starting.
Read ai/handoff.md.

Stage 3 goldens are self-comparison baselines. Stage 4 will likely change rendering
enough that goldens need regeneration — expected and acceptable.

Focus areas:
- Visually compare Drawing/Paths/Misc/Composite output against Quartz2D goldens.
- Fix semantic differences: arc direction, fill rules, coordinate conventions.
- Stroke line width, caps, joins, dash patterns.
- Clipping behavior.
- Alpha compositing edge cases.
- Do NOT attempt text, font metrics, or shadow_style.

After fixing, regenerate Cairo goldens and document what changed visually.
```
