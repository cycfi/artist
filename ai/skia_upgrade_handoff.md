# Skia Upgrade Handoff

**Branch:** `artist_2026_skia_upgrade`
**Last commit:** `3cbf77e`
**Date:** 2026-06-01
**Plan:** `ai/skia_upgrade_plan.md`

---

## Current CI State (run 26716759177)

| Job | Result | Notes |
|-----|--------|-------|
| Skia (macOS Clang) | ❌ test failure | Builds and links correctly; 14/15 pass. Only `composite_ops` fails — pre-existing lighter/darker bug, NOT a regression. ctest exits non-zero. |
| Skia (Ubuntu GCC) | ❌ configure failure | Pre-existing: `external/skia/tools/sk_app/GLWindowContext.cpp` missing (deleted in Skia upgrade). Needs vcpkg port for Linux. |
| Skia (Windows MSVC) | ❌ configure failure | Same pre-existing missing tools files. Needs vcpkg port for Windows. |
| Cairo (macOS Clang) | ✅ | |
| Cairo (Ubuntu GCC) | ✅ | |
| Cairo (Windows MSVC) | ✅ | Configure, build, and tests pass. |
| Quartz2D (macOS Clang) | ✅ | |

---

## Immediate Task: Fix Cairo (Windows MSVC) Build Failures

✅ Done in commits `7ec7eae`, `f30f956`, and `5e19a9e`; handoff updated in `3cbf77e`.

Changes made:
- Replaced Cairo `canvas.cpp` `M_PI` use with a local literal constant.
- Converted `find_file(path_)` result to `.string()` in Cairo `image.cpp` for MSVC.
- Replaced test `M_PI` use in `test/main.cpp` with a local `pi` constant.
- Added `test/windows_golden/cairo/` and wired `ARTIST_CAIRO` on MSVC to those goldens in `test/CMakeLists.txt`.

Local verification:
- `cmake -S . -B build-cairo-local -DARTIST_CAIRO=ON -DARTIST_BUILD_EXAMPLES=OFF -DARTIST_BUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release -G Ninja`
- `cmake --build build-cairo-local`
- `ctest --verbose --test-dir build-cairo-local`

Next milestone: port Skia to Linux and Windows via vcpkg, replacing the `ExternalProject_Add` prebuilt binary path.

---

## Skia (macOS Clang) — composite_ops CI noise

The Skia macOS job shows red because `composite_ops` fails (p95=50.4, pre-existing lighter/darker bug). Build and link are correct; 14/15 tests pass. This is the same result as local. If the red CI noise is distracting, the test can be excluded with `ctest -E composite_ops` in the workflow's Test step — but leave that decision to Joel.

---

## What Was Done (full session summary)

### Skia m148 macOS (commit 2141a03 — prior work)
- Added `lib/external/vcpkg` as a git submodule
- `vcpkg.json`: `skia[gl,metal]` for `"platform": "osx"`
- Full API migration: `SkPathBuilder`, `SkShaders::LinearGradient`, `SkSurfaces::Raster`, `SkPngEncoder::Encode`, per-platform font managers, HarfBuzz SkSpan API
- `examples/host/macos/skia_app.mm` fully rewritten: CAMetalLayer + GrDirectContexts::MakeMetal
- 14/15 tests pass; composite_ops pre-existing bug

### Cairo Windows + CI restructure (commits cc804eb → 772d5e8)

**vcpkg.json** — final state:
```json
{
  "name": "artist",
  "version": "0",
  "dependencies": [
    { "name": "skia", "default-features": true, "features": ["gl", "metal"], "platform": "osx" },
    { "name": "cairo", "features": ["freetype", "fontconfig"], "platform": "windows" },
    { "name": "harfbuzz", "platform": "windows" },
    { "name": "pkgconf", "host": true, "platform": "windows" }
  ]
}
```

**`lib/CMakeLists.txt`** — `ARTIST_CAIRO AND MSVC` block (after `ARTIST_CAIRO AND NOT MSVC` block, around line 312):
```cmake
if (ARTIST_CAIRO AND MSVC)
   # cairo, harfbuzz, and fontconfig are meson-built vcpkg ports; they
   # install pkg-config files only (no cmake config). pkgconf (also in
   # vcpkg.json) provides the pkg-config executable so pkg_check_modules
   # works on Windows the same way it does on Linux and macOS.
   find_package(PkgConfig REQUIRED)
   pkg_check_modules(cairo REQUIRED IMPORTED_TARGET cairo)
   pkg_check_modules(cairo_ft REQUIRED IMPORTED_TARGET cairo-ft)
   pkg_check_modules(harfbuzz REQUIRED IMPORTED_TARGET harfbuzz)
   pkg_check_modules(fontconfig REQUIRED IMPORTED_TARGET fontconfig)
   target_link_libraries(artist PUBLIC
      PkgConfig::cairo PkgConfig::cairo_ft PkgConfig::harfbuzz PkgConfig::fontconfig)
endif()
```

**CI** (`.github/workflows/build_test.yml`) — 7 named jobs:
- `build-skia`: matrix (Windows MSVC / macOS Clang / Ubuntu GCC), `-DARTIST_SKIA=ON`
  - macOS: vcpkg toolchain + `-DVCPKG_TARGET_TRIPLET=arm64-osx` (required — without it vcpkg targets x64-osx on arm64 GitHub runners, producing wrong-arch libraries that link-fail)
- `build-cairo`: matrix (Windows MSVC / macOS Clang / Ubuntu GCC), `-DARTIST_CAIRO=ON`
  - Windows: vcpkg toolchain + `vcpkg_installed` cached on `vcpkg.json` hash; bootstrap via `lib\external\vcpkg\bootstrap-vcpkg.bat`
- `build-quartz2d`: single macOS job, `-DARTIST_QUARTZ_2D=ON`, no vcpkg needed

---

## Key Gotchas for Future Work

1. **vcpkg triplet on GitHub Actions macOS** — always pass `-DVCPKG_TARGET_TRIPLET=arm64-osx` explicitly. Without it vcpkg targets x64-osx, builds x64 libraries, configure succeeds, but link fails with "symbol(s) not found for architecture arm64".

2. **cairo/fontconfig/harfbuzz vcpkg ports are meson-built** — no cmake config files generated. `find_package(unofficial-cairo CONFIG)` will never work. Always use `pkg_check_modules` with `pkgconf` as a vcpkg host tool.

3. **skia[metal] must be platform-guarded** — `"platform": "osx"` in vcpkg.json. Without it vcpkg rejects the manifest on Windows and aborts cmake configure.

4. **HarfBuzz not in vcpkg Skia's cmake link targets** — `libharfbuzz.a` and `libfreetype.a` (+ brotli, bz2) must be found and linked explicitly via `find_library` from `_vcpkg_lib` (derived from the skia include path). See the `APPLE` block in `lib/CMakeLists.txt`.

5. **Layer-hosting NSView does not get `drawRect:`** — in `skia_app.mm`, render is driven explicitly from `-start`, `-on_tick:`, and `-viewDidEndLiveResize`.

6. **Font loading on macOS** — `init_font_map` in `lib/impl/skia/font.cpp` must scan the bundled fonts directory using `makeFromFile` (not CoreText system fonts). Without this, metrics diverge from Cairo, breaking shared test golden values.

7. **`SkPath` is now immutable** — all mutation is via `SkPathBuilder`. `.detach()` for consuming ops, `.snapshot()` for non-consuming.

---

## Next Milestones (in order)

1. ✅ Cairo Windows source/test/golden fixes
2. Port Skia to Linux via vcpkg — replace `ExternalProject_Add` prebuilt binary in `lib/CMakeLists.txt` Linux block; verify fontconfig vcpkg feature builds on Linux
3. Port Skia to Windows via vcpkg — same for MSVC block; write Windows GL/D3D host
4. Fix `composite_ops` lighter/darker blend mode bug (pre-existing in both Skia and Cairo)

---

## Key Header Paths (vcpkg m148, arm64-osx)

| What | Include |
|------|---------|
| `SkCanvas.h`, `SkPath.h`, etc. | `core/Sk*.h` |
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
