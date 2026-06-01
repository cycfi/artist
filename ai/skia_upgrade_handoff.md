# Skia Upgrade Handoff

**Branch:** `artist_2026_skia_upgrade`
**Last commit:** `6b0cbb8`
**Date:** 2026-06-01
**Plan:** `ai/skia_upgrade_plan.md`

---

## Current CI State

| Job | Result | Notes |
|-----|--------|-------|
| Skia (macOS Clang) | ✅ | 15/15 tests pass |
| Skia (Ubuntu GCC) | ❌ configure failure | Pre-existing: `external/skia/tools/sk_app/GLWindowContext.cpp` missing. Needs vcpkg port for Linux. |
| Skia (Windows MSVC) | ❌ configure failure | Same pre-existing missing tools files. Needs vcpkg port for Windows. |
| Cairo (macOS Clang) | ✅ | |
| Cairo (Ubuntu GCC) | ✅ | |
| Cairo (Windows MSVC) | ✅ | |
| Quartz2D (macOS Clang) | ✅ | |

---

## What Was Done (this branch, chronological)

### Skia m148 macOS (commit 2141a03)
- Added `lib/external/vcpkg` as a git submodule
- `vcpkg.json`: `skia[gl,metal]` for `"platform": "osx"` only
- Full API migration: `SkPathBuilder`, `SkShaders::LinearGradient`, `SkSurfaces::Raster`, `SkPngEncoder::Encode`, per-platform font managers, HarfBuzz SkSpan API
- `examples/host/macos/skia_app.mm` fully rewritten: CAMetalLayer + GrDirectContexts::MakeMetal

### Cairo Windows + CI restructure (commits cc804eb → 772d5e8)
- `vcpkg.json`: added `cairo[freetype,fontconfig]`, `harfbuzz`, `pkgconf` (host tool) as Windows-only deps
- `lib/CMakeLists.txt`: `ARTIST_CAIRO AND MSVC` block using `pkg_check_modules` (cairo is meson-built, pkg-config only)
- CI: 7 named jobs — `build-skia` (3-platform matrix), `build-cairo` (3-platform matrix), `build-quartz2d` (macOS only)
- macOS Skia CI: vcpkg toolchain + `-DVCPKG_TARGET_TRIPLET=arm64-osx` (required — without it vcpkg targets x64-osx on arm64 runners)

### Cairo Windows source portability (commits 7ec7eae, f30f956, 5e19a9e — Codex)
- `lib/impl/cairo/canvas.cpp`: replaced `M_PI` with literal constant (MSVC doesn't define it without `_USE_MATH_DEFINES`)
- `lib/impl/cairo/image.cpp`: `find_file(path_).string()` (MSVC `fs::path` has no implicit `std::string` conversion)
- `test/main.cpp`: same `M_PI` fix
- Added `test/windows_golden/cairo/` and wired MSVC Cairo to those goldens in `test/CMakeLists.txt`

### composite_ops regression fix (commit fc710fa)
- **Root cause**: `offscreen_image` used `SkPictureRecorder`. In Skia m148, `drawPicture(picture, matrix, paint)` does not correctly apply Porter-Duff blend modes (kSrcIn, kDstOver, kSrcOut, etc.) during playback — the `saveLayer` restore path changed. Layer blend modes (multiply, screen, etc.) still worked because they go through a different code path.
- **Fix**: `offscreen_image` now uses `SkSurfaces::Raster` instead of `SkPictureRecorder`. On destruction, `makeImageSnapshot()` + `readPixels()` stores a `SkBitmap`. `canvas::draw()` then takes the `drawImageRect` path which honours all `SkBlendMode` values correctly.
- All 15/15 tests now pass (was previously 14/15 — `composite_ops` was the persistent failure attributed to "lighter/darker bug" but was actually a full Porter-Duff regression).

### vcpkg binary caching (commit 6b0cbb8)
- Replaced broken `actions/cache` for `vcpkg_installed/` with vcpkg's native `x-gha` binary cache
- `VCPKG_BINARY_SOURCES: "clear;x-gha,readwrite"` set at job level for `build-skia` and `build-cairo`
- The directory cache never hit because GitHub Actions caches are immutable — if the initial save failed the cache entry never existed. The x-gha binary cache is keyed by ABI hash (compiler + portfile + triplet), survives runner image updates, and is managed by vcpkg directly.
- First run with this change is cold. From the second run onwards, Skia installs from cached archive (~1–2 min) rather than building from source (~15 min).

---

## Key Gotchas

1. **vcpkg triplet on GitHub Actions macOS** — always pass `-DVCPKG_TARGET_TRIPLET=arm64-osx` explicitly. Without it vcpkg targets x64-osx, builds x64 libraries, configure succeeds, link fails with "symbol(s) not found for architecture arm64".

2. **cairo/fontconfig/harfbuzz vcpkg ports are meson-built** — no cmake config files. `find_package(unofficial-cairo CONFIG)` never works. Use `pkg_check_modules` with `pkgconf` as a vcpkg host tool.

3. **skia[metal] must be platform-guarded** — `"platform": "osx"` in vcpkg.json. Without it vcpkg rejects the manifest on Windows.

4. **HarfBuzz not in vcpkg Skia's cmake link targets** — `libharfbuzz.a`, `libfreetype.a`, brotli, bz2 must be found and linked explicitly via `find_library` from `_vcpkg_lib`. See the `APPLE` block in `lib/CMakeLists.txt`.

5. **`offscreen_image` must use a real raster surface** — `SkPictureRecorder` + `drawPicture(..., paint)` breaks Porter-Duff composite ops in Skia m148. Always use `SkSurfaces::Raster`.

6. **Layer-hosting NSView does not get `drawRect:`** — in `skia_app.mm`, render is driven explicitly from `-start`, `-on_tick:`, and `-viewDidEndLiveResize`.

7. **Font loading on macOS** — `init_font_map` in `lib/impl/skia/font.cpp` must scan the bundled fonts directory using `makeFromFile`. Without this, metrics diverge from Cairo and tests fail.

8. **`SkPath` is now immutable** — all mutation via `SkPathBuilder`. `.detach()` for consuming ops, `.snapshot()` for non-consuming.

---

## Next Milestones (in order)

1. ✅ macOS Skia m148 upgrade
2. ✅ Cairo Windows support via vcpkg
3. ✅ CI: 7 named jobs across all backends/platforms
4. ✅ composite_ops Porter-Duff regression fixed — 15/15 tests pass
5. ✅ vcpkg binary caching (x-gha)
6. **Port Skia to Linux** — replace `ExternalProject_Add` prebuilt binary in `lib/CMakeLists.txt` Linux block with vcpkg; verify fontconfig feature builds; add vcpkg bootstrap + triplet to Linux CI Skia job
7. **Port Skia to Windows** — same for MSVC block; write Windows GL/D3D host
8. Fix `composite_ops` lighter/darker approximation — `kDarken` (channel-min) is used for W3C `PlusDarker` (`max(0,Cs+Cd-1)`); no exact Skia equivalent exists; needs a software fallback similar to Cairo's shadow blur

---

## vcpkg.json (current)

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
