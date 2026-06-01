# Skia Linux + Windows Port Plan

**Branch:** `artist_2026_skia_upgrade`
**Date:** 2026-06-01
**Status:** Planning

---

## Goal

Get `Skia (Ubuntu GCC)` and `Skia (Windows MSVC)` CI jobs green, and have
working example apps runnable on an OrbStack Linux VM and a remote Windows
machine.

---

## Current Failures (both platforms)

CMake configure aborts because `lib/CMakeLists.txt` still references files
that were deleted as part of the m148 upgrade:

```
external/skia/tools/sk_app/GLWindowContext.cpp   ← lib, non-Apple
external/skia/tools/sk_app/WindowContext.cpp     ← lib, non-Apple
external/skia/tools/ToolUtils.cpp                ← lib, non-Apple
external/skia/tools/SkMetaData.cpp               ← lib, non-Apple
external/skia/tools/sk_app/win/GLWindowContext_win.cpp   ← lib, Windows
external/skia/tools/sk_app/win/RasterWindowContext_win.cpp
```

Plus: the Linux/Windows blocks still use `ExternalProject_Add` to download
stale prebuilt binary URLs (`prebuilt_binaries_0.90` branch), and the old
`external/skia/include/...` include paths don't exist.

The example hosts have additional issues:
- `examples/host/linux/skia_app.cpp` uses pre-m148 Skia API and `<GL/glx.h>`
  (X11-only, breaks Wayland)
- `examples/host/windows/skia_app.cpp` called into the now-deleted
  `sk_app` context classes

---

## Development Environment

**Linux** — OrbStack VM on macOS. Setup: see `.devcontainer/README.md`.
Once set up, all work happens in a terminal:

```sh
orb shell artist-dev
cd /Users/<you>/dev/cycfi/artist   # macOS files are directly mounted
# edit on macOS, build in the VM
bash .devcontainer/build-skia.sh
./build-skia-linux/examples/shapes/shapes   # Wayland window on macOS desktop
```

**Windows** — remote Windows machine via SSH:

```sh
ssh <user>@<windows-machine>
cd C:\Users\<you>\dev\cycfi\artist
# build with the scripts or cmake directly
```

---

## Work Breakdown

### Step 1 — vcpkg.json: add Skia for Linux and Windows

Add `skia[gl]` for `!osx`. Linux and Windows both use the GL backend.
Metal is macOS-only so the existing osx entry keeps `"features": ["gl","metal"]`.

```json
{
  "name": "skia",
  "default-features": true,
  "features": ["gl", "metal"],
  "platform": "osx"
},
{
  "name": "skia",
  "default-features": true,
  "features": ["gl"],
  "platform": "!osx"
}
```

Note: `fontconfig` vcpkg feature for Skia should work on Linux (the
`gperf` build failure was macOS-only). Verify once the build runs.

---

### Step 2 — lib/CMakeLists.txt: remove deleted sk_app sources

Remove the two `NOT APPLE` blocks that reference deleted files — they are
not needed with vcpkg since we no longer link against sk_app:

```cmake
# DELETE this block entirely:
if (MSVC)
   set(ARTIST_SKIA_PLATFORM_WINDOW_CONTEXT
      external/skia/tools/sk_app/win/GLWindowContext_win.cpp
      external/skia/tools/sk_app/win/RasterWindowContext_win.cpp
   )
endif()

# DELETE this block entirely:
if (NOT APPLE)
   list(APPEND ARTIST_IMPL
      external/skia/tools/sk_app/GLWindowContext.cpp
      external/skia/tools/sk_app/WindowContext.cpp
      external/skia/tools/ToolUtils.cpp
      external/skia/tools/SkMetaData.cpp
   )
endif()
```

---

### Step 3 — lib/CMakeLists.txt: replace Linux ExternalProject_Add with vcpkg

Replace the entire `elseif (UNIX)` block inside `if (ARTIST_SKIA)` with a
pattern mirroring the macOS block. Key differences from macOS:
- No Metal — GL only (`OpenGL::OpenGL`)
- HarfBuzz, FreeType, fontconfig are also available through vcpkg
  (or can stay as system `pkg_check_modules` — Linux system packages are
  fine; vcpkg pulled them in anyway as Skia deps)
- Extra include dirs: derive from `SKIA_INCLUDE_BASE` the same way as macOS,
  adding `ganesh/gl/` for the GL Ganesh headers

```cmake
elseif (UNIX)

   find_package(unofficial-skia CONFIG REQUIRED)

   get_target_property(_skia_inc unofficial::skia::skia INTERFACE_INCLUDE_DIRECTORIES)
   set(SKIA_INCLUDE_BASE "${_skia_inc}")
   set(ARTIST_SKIA_EXTRA_INCLUDES
      ${SKIA_INCLUDE_BASE}/core
      ${SKIA_INCLUDE_BASE}/config
      ${SKIA_INCLUDE_BASE}/codec
      ${SKIA_INCLUDE_BASE}/effects
      ${SKIA_INCLUDE_BASE}/gpu
      ${SKIA_INCLUDE_BASE}/ports
      ${SKIA_INCLUDE_BASE}/utils
      ${SKIA_INCLUDE_BASE}/../harfbuzz
   )

   set(_vcpkg_lib "${SKIA_INCLUDE_BASE}/../../lib")
   find_library(HARFBUZZ_LIB  harfbuzz  PATHS "${_vcpkg_lib}" NO_DEFAULT_PATH REQUIRED)
   find_library(FREETYPE_LIB  freetype  PATHS "${_vcpkg_lib}" NO_DEFAULT_PATH REQUIRED)
   find_library(BROTLIDEC_LIB brotlidec PATHS "${_vcpkg_lib}" NO_DEFAULT_PATH REQUIRED)
   find_library(BROTLICOM_LIB brotlicommon PATHS "${_vcpkg_lib}" NO_DEFAULT_PATH REQUIRED)
   find_library(BZ2_LIB       bz2       PATHS "${_vcpkg_lib}" NO_DEFAULT_PATH REQUIRED)

   find_package(Threads REQUIRED)
   find_package(OpenGL REQUIRED)

   set(ARTIST_DEPEPENDENCIES
      unofficial::skia::skia
      ${HARFBUZZ_LIB}
      ${FREETYPE_LIB}
      ${BROTLIDEC_LIB}
      ${BROTLICOM_LIB}
      ${BZ2_LIB}
      ${CMAKE_DL_LIBS}
      Threads::Threads
      OpenGL::OpenGL
   )

```

Also remove the `add_dependencies(artist prebuilt_binaries)` guard for
non-Apple (line ~315 — only needed when ExternalProject_Add is used).

---

### Step 4 — lib/CMakeLists.txt: replace Windows ExternalProject_Add with vcpkg

Replace the entire `elseif (MSVC)` block inside `if (ARTIST_SKIA)`.
Same pattern as macOS/Linux but Windows-specific link extras
(`opengl32`, etc. are found via `find_package(OpenGL)`):

```cmake
elseif (MSVC)

   find_package(unofficial-skia CONFIG REQUIRED)

   get_target_property(_skia_inc unofficial::skia::skia INTERFACE_INCLUDE_DIRECTORIES)
   set(SKIA_INCLUDE_BASE "${_skia_inc}")
   set(ARTIST_SKIA_EXTRA_INCLUDES
      ${SKIA_INCLUDE_BASE}/core
      ${SKIA_INCLUDE_BASE}/config
      ${SKIA_INCLUDE_BASE}/codec
      ${SKIA_INCLUDE_BASE}/effects
      ${SKIA_INCLUDE_BASE}/gpu
      ${SKIA_INCLUDE_BASE}/ports
      ${SKIA_INCLUDE_BASE}/utils
      ${SKIA_INCLUDE_BASE}/../harfbuzz
   )

   set(_vcpkg_lib "${SKIA_INCLUDE_BASE}/../../lib")
   find_library(HARFBUZZ_LIB  harfbuzz  PATHS "${_vcpkg_lib}" NO_DEFAULT_PATH REQUIRED)
   find_library(FREETYPE_LIB  freetype  PATHS "${_vcpkg_lib}" NO_DEFAULT_PATH REQUIRED)
   find_library(BROTLIDEC_LIB brotlidec PATHS "${_vcpkg_lib}" NO_DEFAULT_PATH REQUIRED)
   find_library(BROTLICOM_LIB brotlicommon PATHS "${_vcpkg_lib}" NO_DEFAULT_PATH REQUIRED)
   find_library(BZ2_LIB       bz2       PATHS "${_vcpkg_lib}" NO_DEFAULT_PATH REQUIRED)

   find_package(OpenGL REQUIRED)

   set(ARTIST_DEPEPENDENCIES
      unofficial::skia::skia
      ${HARFBUZZ_LIB}
      ${FREETYPE_LIB}
      ${BROTLIDEC_LIB}
      ${BROTLICOM_LIB}
      ${BZ2_LIB}
      ${OPENGL_LIBRARIES}
   )

   set(CMAKE_CXX_FLAGS_DEBUG   "${CMAKE_CXX_FLAGS_DEBUG}   /MTd")
   set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /Ob2 /Oi /Ot /GL /MT")
```

---

### Step 5 — lib/CMakeLists.txt: fix non-Apple include paths and SK_GL

The `target_include_directories` `else()` branch (non-Apple Skia) still
references `external/skia/`. Replace with the same vcpkg-derived path as
macOS, using `ARTIST_SKIA_EXTRA_INCLUDES` which is now set for all three
platforms:

```cmake
if (ARTIST_SKIA)
   target_include_directories(artist PUBLIC
      ${ARTIST_SKIA_EXTRA_INCLUDES}
   )
   ...
endif()
```

`SK_GL` define: keep it for Linux. For Windows with GL backend, keep it
too. Remove the `NOT APPLE` guard — it's now correct for both Linux and
Windows:

```cmake
if (ARTIST_SKIA AND NOT APPLE)
   target_compile_definitions(artist PUBLIC SK_GL)
   target_compile_definitions(artist PUBLIC SK_GANESH)
endif()
```

---

### Step 6 — CI: add vcpkg toolchain for Linux and Windows Skia

**Linux** (`build-skia` matrix, Ubuntu GCC entry):
- Add bootstrap step: `lib/external/vcpkg/bootstrap-vcpkg.sh -disableMetrics`
- Add `-DCMAKE_TOOLCHAIN_FILE=lib/external/vcpkg/scripts/buildsystems/vcpkg.cmake`
  to the configure step (via `extra_args` when `runner.os == 'Linux'`)
- No `VCPKG_TARGET_TRIPLET` needed — vcpkg auto-detects `x64-linux`
- `VCPKG_BINARY_SOURCES=clear;x-gha,readwrite` is already set at job level ✓

**Windows** (`build-skia` matrix, Windows MSVC entry):
- Add bootstrap step: `lib\external\vcpkg\bootstrap-vcpkg.bat -disableMetrics`
- Add `-DCMAKE_TOOLCHAIN_FILE=lib/external/vcpkg/scripts/buildsystems/vcpkg.cmake`
  to configure extra_args when `runner.os == 'Windows'`
- `VCPKG_BINARY_SOURCES` already set ✓

At this point the **library and tests should build and pass on both
platforms** (tests are headless — no window host needed).

---

### Step 7 — examples/host/linux/skia_app.cpp: m148 API + Wayland fix

All changes are mechanical translations of the same pattern used in
`examples/host/macos/skia_app.mm`.

**Include fixes** (m148 header paths changed):

| Old | New |
|-----|-----|
| `"GrDirectContext.h"` | `<GrDirectContext.h>` |
| `"gl/GrGLInterface.h"` | `<gl/GrGLInterface.h>` |
| `"gl/GrGLAssembleInterface.h"` | `<gl/GrGLAssembleInterface.h>` |
| `"SkImage.h"` | `<SkImage.h>` |
| `"SkSurface.h"` | `<SkSurface.h>` |
| Add: | `<ganesh/GrDirectContext.h>` (may be needed depending on triplet) |
| Add: | `<ganesh/gl/GrGLBackendSurface.h>` |
| Add: | `<ganesh/SkSurfaceGanesh.h>` |

**Wayland / GLX fix:**

Remove:
```cpp
#include <GL/glx.h>
...
static auto proc = &glXGetProcAddress;
...
// In activate():
if (!proc)
    error("Error: glXGetProcAddress is null");
// In realize() fallback:
state._xface = GrGLMakeAssembledInterface(
    nullptr, (GrGLGetProc)*[](void*, const char* p) -> void* {
        return (void*)glXGetProcAddress((const GLubyte*)p);
    });
```

`GrGLMakeNativeInterface()` is sufficient — GtkGLArea makes the context
current before `realize` fires so the native interface resolves correctly
under both GLX (X11) and EGL (Wayland).

**m148 API fixes** (same pattern as macOS Metal host):

```cpp
// OLD:
GrBackendRenderTarget target(w, h, 0, 8, info);
state._surface = SkSurface::MakeFromBackendRenderTarget(
    state._ctx.get(), target, kBottomLeft_GrSurfaceOrigin,
    colorType, nullptr, nullptr);
state._surface->flush();

// NEW:
auto target = GrBackendRenderTargets::MakeGL(w, h, 0, 8, info);
state._surface = SkSurfaces::WrapBackendRenderTarget(
    state._ctx.get(), target, kBottomLeft_GrSurfaceOrigin,
    colorType, nullptr, nullptr);
state._ctx->flushAndSubmit(state._surface.get());
```

---

### Step 8 — examples/host/windows/skia_app.cpp: new Win32 + WGL host

The existing Windows host called into the deleted sk_app classes. It needs
a full rewrite, following the same pattern as the macOS Metal and Linux GL
hosts but using Win32 + WGL.

Approach: **Win32 window + WGL OpenGL context + Skia Ganesh GL backend**
(same GL backend as Linux, no D3D complexity needed).

Outline:
1. `WinMain` / `wWinMain` entry point (already handled by
   `/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup` in CMakeLists)
2. Register a `WNDCLASS`, create an `HWND`
3. Create a WGL context: `wglCreateContext` + `wglMakeCurrent`
   (or use `wglCreateContextAttribsARB` for a core profile context)
4. `GrGLMakeNativeInterface()` → `GrDirectContexts::MakeGL(xface)`
5. Per-frame: `wglMakeCurrent`, query framebuffer, `GrBackendRenderTargets::MakeGL`,
   `SkSurfaces::WrapBackendRenderTarget`, call `draw(cnv)`,
   `ctx->flushAndSubmit(surface.get())`, `SwapBuffers(hdc)`
6. HiDPI: `GetDpiForWindow` + `SetProcessDpiAwarenessContext`

Reference: `examples/host/macos/skia_app.mm` for the overall structure;
replace Metal API calls 1-for-1 with their WGL/GL equivalents.

---

## Testing Strategy

| Step | Tested via |
|------|-----------|
| 1–5 (CMake wiring) | CI — push and watch `Skia (Ubuntu GCC)` / `Skia (Windows MSVC)` |
| 6 (CI vcpkg) | CI |
| 7 (Linux host) | OrbStack VM — build with examples, run `./build-skia-linux/examples/shapes/shapes` |
| 8 (Windows host) | Remote Windows machine — build with examples, run interactively |
| Elements integration | OrbStack VM + remote Windows machine |

---

## Execution Order

Steps 1–6 are pure CMake/CI changes and can be iterated entirely via CI
pushes from macOS. Steps 7–8 require the respective native environment.

A reasonable approach:
1. Do steps 1–6 together — one or two CI iterations to get tests green
2. Do step 7 in OrbStack (fast local iteration)
3. Do step 8 on the remote Windows machine

---

## Known Unknowns

- **fontconfig vcpkg feature on Linux**: `gperf` was broken on macOS but
  should build on Linux. Verify once the first build runs; fall back to
  `pkg_check_modules(fontconfig)` if the vcpkg port fails.
- **vcpkg Skia triplet on Linux CI**: `x64-linux` is the default; confirm
  the CI runner is x86-64 (it is — `ubuntu-latest` is x86-64).
- **Windows vcpkg Skia GL feature**: the vcpkg port supports `gl` on
  Windows; confirm it links `opengl32` correctly.
- **WGL context version**: some Skia GL paths require OpenGL 3.3+. WGL
  `wglCreateContext` gives a compatibility profile (1.1). Use
  `wglCreateContextAttribsARB` for 3.3 core profile.
