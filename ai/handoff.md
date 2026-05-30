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

## Remaining known limitations

| Area | Status |
|------|--------|
| `shadow_style` | No-op — requires offscreen compositing; deferred (F6) |
| `darker` composite op | Known approximation: `CAIRO_OPERATOR_DARKEN` (channel-min), not W3C PlusDarker. Quartz2D is exact; Cairo/Skia are not. Documented in `canvas.hpp`. |
| Text shaping | No HarfBuzz, no OpenType ligatures, no complex scripts, no bidi; deferred (F7) |
| `image::pixels()` | Returns premultiplied BGRA. Documented in `image.hpp` with per-backend layout table. |

---

## Remaining TODO markers in Cairo source

| File | Description |
|------|-------------|
| `canvas.cpp` — `shadow_style` | Drop-shadow via compositing — deferred |

All other TODOs from the original assimilation have been resolved.

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

- Implement `shadow_style` via offscreen compositing (F6) — significant CPU work,
  needs blur + per-draw-call intercept.
- Add HarfBuzz shaping to Cairo text (F7) — major text engine work.
- Merge `artist_2026_dev` to `master` once branch is considered stable.
