# Skia Upgrade Handoff

**Branch:** `artist_2026_skia_upgrade`
**Commit:** `2141a03`
**Date:** 2026-05-31
**Plan:** `ai/skia_upgrade_plan.md`

---

## Status: macOS COMPLETE

The macOS Skia m148 upgrade is done and squashed into a single commit. All
examples build and render. 14/15 tests pass. The one failure (`composite_ops`)
is a pre-existing lighter/darker blend mode bug, not a regression.

---

## What Was Done

### Infrastructure
- `lib/external/vcpkg` added as git submodule (bootstrapped)
- `vcpkg.json` at repo root: `skia[gl,metal]` — fontconfig removed (gperf build fails on macOS; not needed)
- `.gitignore`: `vcpkg_installed/` ignored (rebuilt on demand, ~8 min first time)
- `CMakeLists.txt` (root): `CMAKE_PREFIX_PATH` fix so `find_package(unofficial-skia)` resolves
- Build command:
  ```
  cmake -S . -B build-skia -DARTIST_SKIA=ON \
    -DCMAKE_TOOLCHAIN_FILE=lib/external/vcpkg/scripts/buildsystems/vcpkg.cmake \
    -DARTIST_BUILD_EXAMPLES=ON -DARTIST_BUILD_TESTS=ON
  cmake --build build-skia
  ```

### lib/CMakeLists.txt (Apple block)
- Replaced `ExternalProject_Add(prebuilt_binaries)` with `find_package(unofficial-skia CONFIG REQUIRED)`
- `ARTIST_DEPEPENDENCIES` = `unofficial::skia::skia` + `libharfbuzz.a` + `Freetype::Freetype` deps found via `find_library`
- `target_link_libraries` uses `PUBLIC` so downstream targets get Skia headers
- Extra include dirs (from `SKIA_INCLUDE_BASE`): `core/`, `config/`, `codec/`, `effects/`, `gpu/`, `ports/`, `utils/`, `../harfbuzz/`
- `SK_GL` guarded to `NOT APPLE` (vcpkg exports it via `INTERFACE_COMPILE_DEFINITIONS`)

### examples/CMakeLists.txt
- Apple+Skia: added `target_link_libraries(${name} "-framework Metal" "-framework QuartzCore")`

### API Migrations

| File | Key Changes |
|------|-------------|
| `lib/include/artist/path.hpp` | `path_impl` for Skia = `class SkPathBuilder` |
| `lib/impl/skia/path.cpp` | Internal `SkPath` → `SkPathBuilder`; `arcTo` → SkPoint args; `addRoundRect` → `SkRRect+addRRect` |
| `lib/impl/skia/canvas.cpp` | `canvas_state::path()` returns `SkPathBuilder&`; fill/stroke/clip use `.detach()`/`.snapshot()`; gradient API → `SkShaders::LinearGradient` etc.; `arcTo` SkPoint args; `add_round_rect_impl` → `SkRRect` |
| `lib/impl/skia/image.cpp` | `SkSurfaces::Raster` include → `<ganesh/SkSurfaceGanesh.h>`; `encodeToData()` → `SkPngEncoder::Encode` |
| `lib/impl/skia/font.cpp` | Font mgr factory per platform; fontconfig guarded to Linux; macOS scans bundled fonts dir via `makeFromFile`+`getFamilyName` (critical — ensures same font files as Cairo for consistent metrics); added `<SkFontTypes.h>` |
| `lib/impl/skia/text_layout.cpp` | `<SkTextBlobBuilder.h>` → `<SkTextBlob.h>`; `MakeFromPosTextH` → `allocRunPosH`; `caret_index` npos bug fixed |
| `lib/impl/skia/detail/harfbuzz.cpp` | `getVariationDesignPosition` → SkSpan API; `SkAutoSTMalloc` → `std::vector` |
| `examples/host/macos/skia_app.mm` | Full rewrite: `CAMetalLayer` + `GrDirectContexts::MakeMetal`; render driven directly (not via `drawRect:`/`setNeedsDisplay:` — layer-hosting views don't get those callbacks) |

### Deleted
- `lib/external/skia/` — 9.6 MB of old vendored Skia headers/sources

---

## Key Lessons / Gotchas

1. **Layer-hosting NSView does not get `drawRect:`** — when `self.layer = _metal_layer` is assigned directly, AppKit never calls `drawRect:`. Render must be driven explicitly: call `[self render]` at the end of `-start`, from `-on_tick:`, and from `-viewDidEndLiveResize`.

2. **Font loading on macOS** — `init_font_map` must scan the bundled fonts directory using `makeFromFile` so the same font files are used as Cairo. Without this, CoreText finds the system version of "Open Sans" (different metrics → different caret positions and text widths, failing tests). The test values (133, 198, etc.) are shared across all backends; never add `#ifdef` guards to paper over metric differences.

3. **HarfBuzz not in vcpkg Skia's link targets** — `libharfbuzz.a` and `libfreetype.a` (+ brotli, bz2) must be found and linked explicitly. The vcpkg Skia cmake target only lists frameworks and image codecs.

4. **fontconfig on macOS** — removed from `vcpkg.json`; `gperf` (its transitive dep) fails to build on macOS. The fontconfig code in `font.cpp` is `#if !defined(__APPLE__) && !defined(_WIN32)` guarded. On macOS, fonts are found via the bundled-font scan + CoreText fallback.

5. **`SkPath` is now immutable** — all mutation methods moved to `SkPathBuilder`. `path_impl` in `path.hpp` changed to `SkPathBuilder`. The `canvas_state` internal path is `SkPathBuilder`, with `.detach()` for consuming ops and `.snapshot()` for non-consuming.

6. **`caret_index` npos bug** — when `lower_bound` on glyph positions returns `end` (`j == l`) on a non-last row, the old code returned `npos`. Fixed to return `glyphs[(i+1)->glyph_index].cluster` (first char of next row).

---

## Test Results

```
ΔE  mean=0.0000  p95=0.0000  tile_margin=-0.5000  (shapes_and_images)  ✅
ΔE  mean=0.0000  p95=0.0000  tile_margin=-0.5000  (shapes2)            ✅
ΔE  mean=0.0000  p95=0.0000  tile_margin=-0.5000  (typography)         ✅
ΔE  mean=4.8270  p95=50.411  tile_margin=52.617   (composite_ops)      ❌ pre-existing bug
ΔE  mean=0.0015  p95=0.0000  tile_margin=-0.3972  (composite_ops2)     ✅
ΔE  mean=0.0000  p95=0.0000  tile_margin=-0.5000  (drop_shadow)        ✅
ΔE  mean=0.0000  p95=0.0000  tile_margin=-0.5000  (tauri)              ✅
ΔE  mean=0.0171  p95=0.1268  tile_margin=-0.3723  (paths)              ✅
ΔE  mean=0.0024  p95=0.0000  tile_margin=-0.4998  (misc)               ✅
ΔE  mean=0.0000  p95=0.0000  tile_margin=-0.5000  (chessboard)         ✅

test cases: 15 | 14 passed | 1 failed
assertions: 224 | 221 passed | 3 failed
```

---

## Next Steps

1. **Fix `composite_ops`** — lighter/darker blend mode bug (pre-existing, separate issue)
2. **Port to Linux** — wire up Linux CMake block to use vcpkg; verify fontconfig feature builds; check system HarfBuzz vs bundled
3. **Port to Windows** — wire up MSVC block; write Windows host or adapt Metal approach to D3D
4. **CI** — bootstrap vcpkg, cache `vcpkg_installed/`, set `CMAKE_TOOLCHAIN_FILE`

---

## Key Header Paths (vcpkg m148, arm64-osx)

| What | Include |
|------|---------|
| `SkCanvas.h`, `SkPath.h`, etc. | `core/Sk*.h` (flat via added include dirs) |
| `SkPathBuilder.h` | `core/SkPathBuilder.h` |
| `SkTextBlob.h` + builder | `core/SkTextBlob.h` |
| `SkGradient` + `SkShaders` | `effects/SkGradient.h` |
| `SkSurfaces::WrapBackendRenderTarget` | `<ganesh/SkSurfaceGanesh.h>` |
| `GrDirectContexts::MakeMetal` | `<ganesh/mtl/GrMtlDirectContext.h>` |
| `GrBackendRenderTargets::MakeMtl` | `<ganesh/mtl/GrMtlBackendSurface.h>` |
| `SkFontMgr_New_CoreText` | `<ports/SkFontMgr_mac_ct.h>` |
| `SkPngEncoder::Encode` | `<encode/SkPngEncoder.h>` |
| HarfBuzz `hb.h` | `../harfbuzz/hb.h` (relative to skia include base) |
| `SkRRect` | `<SkRRect.h>` |

CMake target: `unofficial::skia::skia` from `find_package(unofficial-skia CONFIG REQUIRED)`
