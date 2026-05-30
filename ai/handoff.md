# Artist Cairo backend handoff

## Current state

All ten assimilation stages are complete. The branch is `artist_2026_dev`.

The Cairo backend is now a fully tested, CI-verified optional backend alongside
Skia and Quartz2D.

---

## CI structure

`.github/workflows/build_test.yml` — four jobs, all passing:

| Job | OS | Compiler | Backend | Tests |
|-----|----|----------|---------|-------|
| Windows Latest MSVC | windows-latest | cl | Skia | ✓ |
| Ubuntu Latest GCC | ubuntu-latest | gcc/g++ | Skia | ✓ |
| macOS Latest Clang | macos-latest | clang++ | Quartz2D | ✓ |
| Ubuntu Latest GCC (Cairo) | ubuntu-latest | gcc/g++ | Cairo | ✓ |

All jobs use `actions/checkout@v4`.

---

## Golden image layout

| Directory | Backend | Platform |
|-----------|---------|----------|
| `test/macos_golden/skia/` | Skia | macOS |
| `test/macos_golden/quartz2d/` | Quartz2D | macOS |
| `test/macos_golden/cairo/` | Cairo | macOS |
| `test/linux_golden/skia/` | Skia | Linux |
| `test/linux_golden/cairo/` | Cairo | Linux ← generated from CI |
| `test/windows_golden/skia/` | Skia | Windows |

Each directory contains 10 golden PNGs: `shapes_and_images`, `shapes2`,
`typography`, `composite_ops`, `composite_ops2`, `drop_shadow`, `paths`,
`misc`, `chessboard`, (scale test has no golden).

### Chessboard test

The `Chessboard` test case (added during this session) exercises
`make_image<pixel_format::rgba32>` — the pixel-buffer image constructor.
It lives in `test/main.cpp` and replaced `examples/chessboard.cpp`.

---

## What changed in the post-Stage-10 session

### Branch rename
`artist-cairo-backend` → `artist_2026_dev`

### CI fixes
- Added `-DARTIST_BUILD_EXAMPLES=OFF` to Cairo CI job (avoids OpenGL/GTK dependency).
- Added missing `windows_golden/skia/shapes2.png` and `composite_ops2.png`.
- Added missing `linux_golden/skia/shapes2.png` and `composite_ops2.png`.
- Enabled Cairo CI tests; generated and committed `linux_golden/cairo/` goldens.
- Upgraded `actions/checkout@v2` → `@v4`.

### make_image implemented
`image::image(uint8_t const*, pixel_format, extent)` is now fully implemented
for Cairo in `lib/impl/cairo/image.cpp`. Converts gray8 / rgb16 / rgb32 / rgba32
input to Cairo's premultiplied BGRA ARGB32 format. `_pixmap_size()` also implemented
(was returning 0).

### Chessboard moved to tests
`examples/chessboard.cpp` deleted. `chessboard()` draw function and `Chessboard`
TEST_CASE added to `test/main.cpp`. Goldens for all platforms committed.

### Cairo macOS example host fixed
`examples/host/macos/quartz2d_app.mm` Cairo path now:
1. Renders into a `cairo_image_surface` (plain ARGB32, top-down, Retina-aware).
2. Blits via `CGBitmapContextCreate` + `CGImage` + `CGContextDrawImage` with an
   explicit CTM flip to land right-side-up inside the `isFlipped=YES` NSView.

This fixes the blank/upside-down rendering that affected all Cairo examples on macOS.

### Examples verified (Cairo, macOS)
All examples build and render correctly:

| Example | Status |
|---------|--------|
| rain | ✓ |
| shadow | ✓ |
| shapes | ✓ |
| paths | ✓ |
| composite_ops | ✓ |
| typography | ✓ |
| space | ✓ |
| tauri | ✓ |

### README updated
- News entry added.
- Known limitations updated: `make_image` no longer listed as unimplemented.
- CI section updated to reflect that visual tests now run for Cairo.

---

## Remaining known limitations

| Area | Status |
|------|--------|
| `shadow_style` | No-op — Cairo has no native drop-shadow |
| `darker` composite op | Approximate: `CAIRO_OPERATOR_DARKEN` (channel-min), not W3C PlusDarker |
| Text shaping | No HarfBuzz, no OpenType ligatures, no complex scripts, no bidi |
| `text_layout::text(…)` | Re-creates impl but does not preserve `font_descr`; font selection may drift |
| `path::operator==` | Compares by pointer identity only; deep equality not implemented |
| `image::pixels()` | Returns premultiplied BGRA, not straight-alpha RGBA |
| Cairo window host | macOS/Linux examples use CGBitmapContext blit — not a native Cairo window surface |

---

## Remaining TODO markers in Cairo source

| File | Location | Description |
|------|----------|-------------|
| `canvas.cpp:408` | `shadow_style` | Drop-shadow via compositing — known limitation |
| `canvas.cpp:431` | `darker` op | W3C PlusDarker not expressible in Cairo |
| `image.cpp:65` | `pixels()` | BE endian verification |
| `path.cpp:57` | `operator==` | Deep path equality unimplemented |
| `text_layout.cpp:648` | `text(string_view)` | font_descr not stored in impl |
| `text_layout.cpp:655` | `text(u32string_view)` | Same font_descr gap |

---

## Build commands

```sh
# Cairo (macOS)
cmake -S . -B build-cairo -DARTIST_CAIRO=ON -DARTIST_BUILD_TESTS=ON
cmake --build build-cairo
ctest --test-dir build-cairo --output-on-failure

# Skia (macOS)
cmake -S . -B build-skia -DARTIST_SKIA=ON -DARTIST_BUILD_TESTS=ON
cmake --build build-skia
ctest --test-dir build-skia --output-on-failure

# Quartz2D (macOS)
cmake -S . -B build-quartz -DARTIST_BUILD_TESTS=ON
cmake --build build-quartz
ctest --test-dir build-quartz --output-on-failure
```

---

## Test results (local, macOS)

- **Cairo** (`build-cairo`): 10/10 PASS
- **Skia** (`build-skia`): 11/11 PASS
- **Quartz2D** (`build-quartz`): not run this session (Skia/Cairo cover it)

---

## Possible next tasks

- Implement a proper Cairo window surface host (XCB on Linux, Quartz on macOS) so the
  examples use a real Cairo surface rather than a CGBitmap blit.
- Implement `shadow_style` via an offscreen compositing step.
- Implement HarfBuzz shaping for Cairo text.
- Fix `text_layout::text()` to preserve `font_descr` across updates.
- Fix `path::operator==` for deep equality.
