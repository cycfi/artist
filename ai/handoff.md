# Artist Cairo backend handoff

## Current state

Branch: `artist_2026_dev`. All Cairo follow-up stages F0–F5, F8–F10 are complete.
The Cairo backend is a fully tested, CI-verified optional backend alongside Skia and
Quartz2D, with live window hosts on all three platforms.

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

### macOS Cairo host
Renders into a `cairo_image_surface` (ARGB32, Retina-aware), then blits via
`CGBitmapContextCreate` + `CGImage` + `CGContextDrawImage` with a CTM flip to
land right-side-up inside the `isFlipped=YES` NSView.

### Linux Cairo host
Uses a `GtkDrawingArea`. `on_configure` creates a `gdk_window_create_similar_surface`
backing surface. `on_draw` blits it then calls `draw(cr)` with the GTK-provided
`cairo_t*` directly. No GL required.

### Windows Cairo host
Uses a Win32 child window with an offscreen HDC for double-buffering.
`WM_PAINT` creates `cairo_win32_surface_create(offscreen_hdc)`, calls `draw(cnv)`,
destroys both, then `BitBlt`s to screen. DPI scaled via `GetDpiForWindow`.

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
`typography`, `composite_ops`, `composite_ops2`, `drop_shadow`, `paths`,
`misc`, `chessboard`.

---

## Completed follow-up stages

| Stage | Description | Commit |
|-------|-------------|--------|
| F0 | Reconnaissance — `make_image` confirmed, limitations verified | — |
| F1 | `text_layout::text()` preserves `font_descr` | `b10ce70` |
| F2 | `path::operator==` deep structural equality | `038347a` |
| F3 | `image.hpp` pixel layout contract documented for all backends | `400450e` |
| F4 | Linux Cairo goldens committed; Cairo CI tests mandatory | `92130fa` |
| F5 | `darker` approximation locked in; `canvas.hpp` documents cross-backend policy | `1fd7a15` |
| F8 | macOS Cairo window host (`cairo_app.mm`) separated from `quartz2d_app.mm` | `02dc969` |
| F9 | Linux Cairo window host (`cairo_app.cpp`) | `ad6cc7f` |
| F10 | Windows Cairo window host (`cairo_app.cpp`) | `ad6cc7f` |

---

## Completed follow-up stages (continued)

| Stage | Description | Commits |
|-------|-------------|---------|
| F6 | `shadow_style` implemented via software Gaussian blur + offscreen compositing | `1cc99c5`…`e48218e` |

### F6 implementation details

`shadow_style` is now fully implemented in `lib/impl/cairo/canvas.cpp`. Key design decisions:

- **Intercept point**: `fill()`, `stroke()`, `fill_preserve()`, `stroke_preserve()` call `draw_shadow()` before the actual draw if `shadow_info.active`.
- **Shadow surface**: user-space ARGB32 temp surface (scratch buffers in `canvas_state` — never reallocate unless surface grows).
- **Alpha-only blur**: extracts the single alpha byte per pixel, blurs a `w×h` byte buffer (4× smaller than ARGB32), writes back. 3-pass box blur via transpose trick (cache-friendly vertical pass).
- **Sigma scaling**: `sigma = blur * 0.5 / backing_scale` where `backing_scale = min(CTM_scale, 2.0)`. At 2× Retina, bilinear upscaling of the user-space surface adds free softness, so sigma is halved per backing-scale unit. User-applied `scale()` transforms are excluded from the divisor so shadow blur grows proportionally with user coordinates (matching Quartz behaviour).
- **Offset**: applied in device-pixel space (divided by CTM scale), matching Quartz `CGContextSetShadowWithColor` semantics which use the base coordinate space.
- **Margin**: `ceil(blur * 1.5)` user pixels — independent of sigma, so the feather ring stays adequately large at any scale.
- **Golden**: `test/macos_golden/cairo/drop_shadow.png` updated; SSI=1.0 vs the new golden, pixel-level match with Quartz2D on key shadow pixels.

### F6 known remaining limitation

Shadow performance degrades for very large shapes: the blur processes the full surface area including the opaque interior. An attempt at a border-only optimisation was reverted due to artifacts. For typical widget shadows (small shapes, blur 4–8px) performance is adequate. Large animated shadows (e.g. shapes growing to 600×440px) show measurable slowdown. Future work: implement a correct border-only blur or cap surface size.

---

## Remaining known limitations

| Area | Status |
|------|--------|
| `shadow_style` performance | Correct for widget shadows; faster than original after SIMD optimisation. Very large shapes still slower than GPU-backed Quartz2D/Skia. |
| `darker` composite op | Known approximation: `CAIRO_OPERATOR_DARKEN` (channel-min), not W3C PlusDarker. Quartz2D is exact; Cairo/Skia are not. Documented in `canvas.hpp`. |
| Text shaping | No HarfBuzz, no OpenType ligatures, no complex scripts, no bidi; deferred (F7) |
| `image::pixels()` | Returns premultiplied BGRA. Documented in `image.hpp` with per-backend layout table. |

---

## Remaining TODO markers in Cairo source

None. All TODOs from the original assimilation and follow-up stages have been resolved.

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

## Completed follow-up stages (continued)

| Stage | Description |
|-------|-------------|
| F11 | SIMD shadow blur — refactored into `shadow_blur.hpp/.cpp`; NEON + SSE2 + scalar fallback; prefix-sum box blur; `shadow_reconstruct` for correct premultiplied color |
| F12 | `fill_text` shadow support — text shadows via `cairo_text_path` + `draw_shadow` + `cairo_fill` |
| F13 | `shadow_reconstruct` — fixed glow/coloured shadows (RGB was left as 0 outside glyphs after alpha-only blur; now writes premultiplied shadow colour × blurred alpha) |
| F14 | Sigma calibration — sigma scaling reworked to match `CGContextSetShadowWithColor` semantics |
| F15 | Device-resolution shadow rendering — fixes over-wide/over-soft glow at large user scales (e.g. `cnv.scale(10,10)` in tauri); shadow surface now rendered at `ctm_scale * device_scale` and composited via a pattern matrix |

### F11–F15 implementation details

**`lib/impl/cairo/shadow_blur.hpp/.cpp`** — new utility file:
- `alpha_extract`: NEON `vld4q_u8` (16px/iter) / SSE2 `srli_epi32`+pack (16px/iter) / scalar
- `alpha_writeback`: NEON `vst4q_u8` (16px/iter) / SSE2 unpack+mask+OR (4px/iter) / scalar
- `shadow_reconstruct`: premultiplied BGRA reconstruction from blurred alpha × shadow colour — NEON `vmull_u8`/`vrshrn_n_u16`/`vst4_u8` (8px/iter) / SSE2 `mullo_epi16`+interleave (8px/iter) / scalar
- `box_blur_h_1ch`: prefix-sum approach — interior loop is branch-free multiply, SIMD-vectorised (4px/iter both NEON and SSE2)
- `transpose_1ch`: 16×16 cache-blocked scalar
- `gaussian_blur_1ch`: 3-pass box blur via transpose (unchanged API, new `cum` parameter)

**`canvas.cpp`** changes:
- `shadow_scratch_t` gains `std::vector<int32_t> cum` (prefix-sum scratch, avoids per-call alloc)
- `draw_shadow`: calls `alpha_extract` → `gaussian_blur_1ch` → `shadow_reconstruct` (was scalar loops + `alpha_writeback`)
- `fill_text`: shadow path uses `cairo_text_path` + `draw_shadow` + `cairo_fill`; non-shadow path unchanged (`cairo_show_text`)
- **Device-resolution rendering (F15)**: the shadow surface is rendered at `render_scale = ctm_scale * device_scale` (full physical resolution), not user-space resolution. This is the key fix for the over-wide glow: at large user scales (e.g. `scale(10,10)`), the old user-space surface produced sub-pixel sigma that clamped to box-blur radius 1 (≈1.4px sigma) and was then bilinear-upscaled 10×, yielding ~14px device sigma instead of the intended 5px. `sigma = blur * 0.5 * device_scale` in surface px (physical blur scales with Retina only, NOT user scale — matching Quartz). Composited via `cairo_pattern_set_matrix` mapping current user space → surface px, so the shadow lands exactly at the shape (shifted by offset) and samples 1:1 in device space (crisp). `margin = ceil(sigma*3)+2` surface px.
- Tauri example added to the test suite (`TEST_CASE("Tauri")`) with goldens for all three backends; exercises shadow + glow at `scale(10,10)` plus a stroked sub-path (verifies `line_width` propagation to the scratch context).

## Possible next tasks

- Add HarfBuzz shaping to Cairo text (F7) — major text engine work.
- `stroke_text` shadow support — currently calls `stroke()` which has shadow support, but `stroke_text` via text_layout path does not.
- Merge `artist_2026_dev` to `master` once branch is considered stable.
