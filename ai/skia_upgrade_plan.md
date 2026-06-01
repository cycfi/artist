# Skia Upgrade Plan: vcpkg + API Migration

**Status: macOS COMPLETE** — branch `artist_2026_skia_upgrade`, commit `2141a03`

---

## Objective

Replace the current hand-managed prebuilt Skia binary system with a vcpkg-based dependency, and migrate the Skia backend source to the current public API (m148). vcpkg is added as a git submodule at `lib/external/vcpkg/` so it is pinned, versioned with the repo, and can manage all artist dependencies.

---

## Why vcpkg as a submodule

The existing approach downloads pre-built `.a`/`.zip` archives hosted on an ad-hoc GitHub branch (`prebuilt_binaries_0.90`). This creates supply-chain risk, requires manual rebuilds for every platform/compiler/arch combination, and the binary URLs have already started going stale.

vcpkg as a submodule: pins the exact vcpkg commit alongside the source, builds Skia from source with the correct compiler/flags, handles the GN→CMake bridge, exposes a standard `find_package` interface.

---

## Execution Status

| Step | Status |
|------|--------|
| 1. Add vcpkg as submodule | ✅ Done — `lib/external/vcpkg/` |
| 2. Add `vcpkg.json` (skia[gl,metal]) | ✅ Done |
| 3. Wire up `lib/CMakeLists.txt` for macOS | ✅ Done |
| 4. Fix API in impl files | ✅ Done — see API Migration section |
| 5. Fix `text_layout.cpp` | ✅ Done |
| 6. Write macOS Metal host | ✅ Done — CAMetalLayer + GrDirectContexts::MakeMetal |
| 7. Delete `lib/external/skia/` | ✅ Done |
| 8. Test — `artist_test`, examples | ✅ 14/15 tests pass; all examples render |
| 9. Port to Linux | ⬜ TODO |
| 10. Port to Windows | ⬜ TODO |
| 11. Update CI | ⬜ TODO |

---

## Build Instructions (macOS)

```sh
# One-time vcpkg bootstrap (already done in submodule):
lib/external/vcpkg/bootstrap-vcpkg.sh -disableMetrics

# Configure + build:
cmake -S . -B build-skia -DARTIST_SKIA=ON \
  -DCMAKE_TOOLCHAIN_FILE=lib/external/vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DARTIST_BUILD_EXAMPLES=ON -DARTIST_BUILD_TESTS=ON
cmake --build build-skia
```

---

## vcpkg port facts (m148)

- **Package**: `unofficial-skia`, version `148`
- **CMake target**: `unofficial::skia::skia`
- **Features installed**: `gl`, `metal`, `harfbuzz`, `icu`, `jpeg`, `png`, `webp`, `dng`
- **Include base**: `vcpkg_installed/arm64-osx/include/skia/`
- **Extra link deps** (not in skia cmake target): `libharfbuzz.a`, `libfreetype.a`, `libbrotlidec.a`, `libbrotlicommon.a`, `libbz2.a` — linked explicitly via `find_library` in `lib/CMakeLists.txt`
- **HarfBuzz headers**: `vcpkg_installed/arm64-osx/include/harfbuzz/` — added to include path as `../harfbuzz` relative to skia base
- **`tools/` headers**: NOT installed — `skia_app.mm` fully rewritten, no dependency on `sk_app`

---

## API Migration (m128→m148)

### path.hpp / path.cpp / canvas.cpp
- `path_impl` for Skia changed from `class SkPath` to `class SkPathBuilder` (SkPath is now immutable)
- `canvas_state::path()` returns `SkPathBuilder&`
- `fill/stroke/clip` use `.detach()`, `fill_preserve/stroke_preserve` use `.snapshot()`
- `arcTo(x1,y1,x2,y2,r)` → `arcTo({x1,y1},{x2,y2},r)` (SkPoint args)
- `addRoundRect` → `SkRRect::setRectXY` + `addRRect`

### image.cpp
- `SkSurface::MakeRasterN32Premul` → `SkSurfaces::Raster(SkImageInfo::MakeN32Premul(...))`
- Include: `"SkSurfaces.h"` → `<ganesh/SkSurfaceGanesh.h>`
- `image->encodeToData()` → `SkPngEncoder::Encode(nullptr, image.get(), {})` + `<encode/SkPngEncoder.h>`

### canvas.cpp (gradients)
- `#include <SkGradientShader.h>` → `#include <effects/SkGradient.h>`
- `SkGradientShader::MakeLinear/MakeTwoPointConical` → `SkShaders::LinearGradient/TwoPointConicalGradient` using `SkGradient` struct

### text_layout.cpp
- `<SkTextBlobBuilder.h>` → `<SkTextBlob.h>` (builder defined there in m148)
- `SkTextBlob::MakeFromPosTextH` removed → `SkTextBlobBuilder::allocRunPosH`
- `caret_index`: fixed `npos` return when `j==l` on non-last row — now returns next row's first cluster

### font.cpp
- `SkTypeface::MakeFromFile/Name` → `get_font_mgr()->makeFromFile/matchFamilyStyle`
- Platform font managers: `SkFontMgr_New_CoreText` (Apple), `SkFontMgr_New_GDI` (Win), `SkFontMgr_New_FontConfig` (Linux)
- fontconfig `#include` and `init_font_map` body guarded `#if !defined(__APPLE__) && !defined(_WIN32)`
- **On macOS**: `init_font_map` scans the bundled fonts directory using `makeFromFile` + `getFamilyName` so the same font files are used as Cairo — critical for metric agreement across backends
- Added `<SkFontTypes.h>` for `SkTextEncoding`

### harfbuzz.cpp
- `getVariationDesignPosition(nullptr, 0)` two-arg API → `getVariationDesignPosition({})` (SkSpan)
- `SkAutoSTMalloc` removed → `std::vector`

### skia_app.mm (macOS host)
- Full rewrite: removed `tools/sk_app` dependency
- Uses `CAMetalLayer` + `GrDirectContexts::MakeMetal` (Metal backend — no deprecated OpenGL)
- Per-frame: `nextDrawable` → `GrBackendRenderTargets::MakeMtl` → `SkSurfaces::WrapBackendRenderTarget` → draw → `flushAndSubmit` → `presentDrawable`
- **Layer-hosting fix**: a view that assigns `self.layer` directly never gets `drawRect:` — rendering is now driven by calling `render` directly from `-start`, `-on_tick:`, and `-viewDidEndLiveResize`

---

## Test Results (macOS, arm64)

- **14/15 test cases pass** — `artist_test`
- Failing: `composite_ops` (ΔE p95=50) — pre-existing lighter/darker blend mode bug, not a regression
- Golden images regenerated for: `shapes_and_images`, `shapes2`, `tauri`, `typography`

---

## Remaining Work

### Linux
- Wire up `lib/CMakeLists.txt` Linux block to use vcpkg Skia (currently still uses `ExternalProject_Add` + prebuilt binary)
- vcpkg fontconfig: `gperf` fails to build on macOS but should work on Linux — test
- Verify fontconfig feature flag in vcpkg for Linux skia build

### Windows
- Wire up `lib/CMakeLists.txt` Windows/MSVC block similarly
- Write Windows GL context replacement or port Metal approach to D3D

### CI
- Bootstrap vcpkg in CI and pass `CMAKE_TOOLCHAIN_FILE`
- Cache `vcpkg_installed/` between runs (keyed on `vcpkg.json` hash)
- Add `VCPKG_BINARY_SOURCES` for binary caching

---

## Known Issues / Notes

- **`composite_ops` test**: lighter/darker composite op bug is pre-existing in Skia (separate fix needed)
- **fontconfig on macOS**: removed from `vcpkg.json` — `gperf` build fails; not needed since macOS uses CoreText + bundled font scan
- **Vendored `lib/external/skia/`**: deleted — the old headers are gone
- **HarfBuzz vendored copy** (`lib/external/harfbuzz/`): still present for Linux/Cairo backend; should eventually be replaced by vcpkg-managed HarfBuzz
