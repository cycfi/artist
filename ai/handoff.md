# Artist Cairo backend handoff

## Current state

Branch: `artist_2026_dev`. All Cairo follow-up stages F0–F10 plus F6 (shadow)
and F7 (HarfBuzz text shaping) are complete. The Cairo backend is a fully
tested, CI-verified optional backend alongside Skia and Quartz2D, with live
window hosts on all three platforms.

---

## CI structure

`.github/workflows/build_test.yml` — four jobs, all passing:

| Job | OS | Compiler | Backend | Tests |
|-----|----|----------|---------|-------|
| Windows Latest MSVC | windows-latest | cl | Skia | ✓ |
| Ubuntu Latest GCC | ubuntu-latest | gcc/g++ | Skia | ✓ |
| macOS Latest Clang | macos-latest | clang++ | Quartz2D | ✓ |
| Ubuntu Latest GCC (Cairo) | ubuntu-latest | gcc/g++ | Cairo | ✓ |

All jobs use `actions/checkout@v4`. Each job uploads `test-results-*` artifacts.

---

## Window host layout

One dedicated file per backend per platform. CMakeLists selects the right one.

| File | Backend | Platform |
|------|---------|----------|
| `examples/host/macos/cairo_app.mm` | Cairo | macOS |
| `examples/host/macos/quartz2d_app.mm` | Quartz2D | macOS |
| `examples/host/macos/skia_app.mm` | Skia | macOS |
| `examples/host/linux/cairo_app.cpp` | Cairo | Linux/GTK |
| `examples/host/linux/skia_app.cpp` | Skia | Linux/GTK+GL |
| `examples/host/windows/cairo_app.cpp` | Cairo | Windows/Win32 |
| `examples/host/windows/skia_app.cpp` | Skia | Windows/GL |

---

## Golden image layout

| Directory | Backend | Platform |
|-----------|---------|----------|
| `test/macos_golden/skia/` | Skia | macOS |
| `test/macos_golden/quartz2d/` | Quartz2D | macOS |
| `test/macos_golden/cairo/` | Cairo | macOS |
| `test/linux_golden/skia/` | Skia | Linux |
| `test/linux_golden/cairo/` | Cairo | Linux |
| `test/windows_golden/skia/` | Skia | Windows |

Each directory contains 10 golden PNGs: `shapes_and_images`, `shapes2`,
`typography`, `composite_ops`, `composite_ops2`, `drop_shadow`, `tauri`,
`paths`, `misc`, `chessboard`.

---

## Visual regression testing scheme

`test/main.cpp` — see the block comment near "Test Drivers" for full
documentation. Summary:

Two complementary checks in `compare_golden()`:

1. **Global CIEDE2000 stats** (`compare_images`): convert every pixel to CIELAB
   and compute ΔE₀₀. Thresholds: `mean < 1.0`, `p95 < 3.0`.

2. **Complexity-adaptive tiled check** (`check_tiles`): divide into 32×32 tiles,
   classify each by luminance variance of the golden tile, apply a matching
   ΔE tolerance:

   | Tile type | Luminance variance | ΔE tolerance |
   |---|---|---|
   | Simple (uniform) | < 50 | 0.5 |
   | Moderate (edges/gradients) | 50–500 | 2.0 |
   | Complex (detail/text) | ≥ 500 | 5.0 |

   The tight tolerance on simple tiles catches small localised regressions
   (e.g. shifted glyph, wrong fill colour) that would be invisible to the
   global mean.

**Updating goldens**: render, inspect the PNG in `results/`, copy to the
appropriate `*_golden/<backend>/` directory, re-run CMake, confirm green.
Never lower thresholds to paper over a regression.

---

## Completed follow-up stages

| Stage | Description | Commit |
|-------|-------------|--------|
| F0 | Reconnaissance — `make_image` confirmed, limitations verified | — |
| F1 | `text_layout::text()` preserves `font_descr` | squashed |
| F2 | `path::operator==` deep structural equality | squashed |
| F3 | `image.hpp` pixel layout contract documented for all backends | squashed |
| F4 | Linux Cairo goldens committed; Cairo CI tests mandatory | squashed |
| F5 | `darker` approximation locked in; `canvas.hpp` cross-backend policy | squashed |
| F6 | `shadow_style` via software Gaussian blur + offscreen compositing | `be4e0f7` |
| F7 | HarfBuzz text shaping | `00dcfdf` |
| F8 | macOS Cairo window host (`cairo_app.mm`) | squashed |
| F9 | Linux Cairo window host (`cairo_app.cpp`) | squashed |
| F10 | Windows Cairo window host (`cairo_app.cpp`) | squashed |

---

## F6 — shadow_style implementation details

`shadow_style` is fully implemented in `lib/impl/cairo/canvas.cpp`.

- **Intercept**: `fill()`, `stroke()`, `fill_text()` etc. call `draw_shadow()`
  before the actual draw when `shadow_info.active`.
- **Blur**: `lib/impl/cairo/shadow_blur.hpp/.cpp` — SIMD-optimised (NEON/SSE2)
  3-pass box blur via transpose trick. Alpha-only pass (4× smaller buffer).
  `shadow_reconstruct` writes premultiplied shadow colour × blurred alpha back.
- **Device-resolution rendering**: shadow surface rendered at
  `render_scale = ctm_scale * device_scale`. Composited via
  `cairo_pattern_set_matrix` so sigma tracks Retina scale only, not user scale,
  matching Quartz `CGContextSetShadowWithColor` semantics.
- **Known limitation**: very large shapes (e.g. 600×440 px) are slower than
  GPU-backed backends. A border-only optimisation was attempted but reverted
  due to artifacts.

---

## F7 — HarfBuzz text shaping implementation details

`lib/impl/cairo/cairo_text.hpp` — `shape_text()` helper:

- Creates an `hb_buffer_t`, feeds UTF-8 or UTF-32, calls `hb_shape()` with
  `hb_ot_font_set_funcs` so GSUB/kern/liga OpenType features activate.
- Returns a `shaped_run` (glyph IDs, cluster indices, x_advances in pixels).

`lib/impl/cairo/font.cpp`:
- `cairo_font_impl` exposes `hb_font_t*`; scale set to `(upem, upem)`.

`lib/impl/cairo/canvas.cpp`:
- `fill_text` / `stroke_text` use shaped glyph IDs and advances.
- `measure_text` uses shaped advances so widths match rendering.

`lib/impl/cairo/text_layout.cpp`:
- Rewritten to use HarfBuzz-shaped runs for caret mapping, line breaking, and
  flow. Ligature clusters handled: `caret_point()` interpolates within a
  multi-codepoint cluster.

---

## Unit tests (`test/main.cpp`)

Beyond the 10 visual golden-image regression tests, the following unit test
cases assert logical correctness:

| Test case | Assertions | What it tests |
|---|---|---|
| `Path Equality` | 8 | `path::operator==`: geometry, fill rule, SVG round-trip |
| `Image Pixel Round Trip` | 22 | `make_image<rgba32>` → `pixels()` raw byte preservation |
| `Text Shaping` | 16 | measure_text widths, ligature invariant, `num_lines()`, `caret_point()` monotonicity |
| `Scale and Coordinate Conversion` | 3 | `user_to_device` / `device_to_user` inverse round-trip |
| `Color Maths` | 4 | `color` arithmetic operators |

---

## Remaining known limitations

| Area | Status |
|------|--------|
| `shadow_style` performance | Correct for widget shadows; very large shapes slower than GPU-backed backends |
| `darker` composite op | Approximation: `CAIRO_OPERATOR_DARKEN` (channel-min), not W3C PlusDarker. Documented in `canvas.hpp` |
| `text_layout::text()` | Does not preserve `font_descr` across updates — deferred |
| `image::pixels()` | Returns premultiplied BGRA. Documented in `image.hpp` with per-backend layout table |
| `image::pixels()` on Quartz2D | Works for all image types including `make_image` (lazy `initWithCGImage:` path) |

---

## Remaining TODO markers in Cairo source

None.

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

## Possible next tasks

- Merge `artist_2026_dev` to `master` once branch is considered stable.
- Implement `stroke_text` shadow support via `text_layout` path.
- `text_layout::text()` — preserve `font_descr` across updates.
- Implement `shadow_style` border-only blur optimisation for large shapes.
