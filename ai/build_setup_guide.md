# Artist Build & Setup Guide

This document covers build instructions and setup notes for all supported
platforms and architectures.  It is updated as new findings are confirmed
during development and verification.

---

## Platform / Architecture Matrix

| Platform | Arch | Backend | Status | Notes |
|----------|------|---------|--------|-------|
| macOS 14+ | arm64 | Skia | ✅ Working | vcpkg arm64-osx |
| macOS 14+ | arm64 | Quartz2D | ✅ Working | Native |
| macOS 14+ | arm64 | Cairo | ✅ Working | Homebrew deps |
| Ubuntu 24.04 | x86-64 | Skia | ✅ CI green (headless) | Window host untested |
| Ubuntu 24.04 | x86-64 | Cairo | ✅ CI green | |
| Ubuntu 24.04 | arm64 | Skia | ❌ harfbuzz 8.3.0 bug | GTK crashes on widget realize |
| Ubuntu 26.04 LTS | arm64 | Skia | ✅ Working | harfbuzz 12.3.2; Mesa EGL warnings harmless |
| Windows 10/11 | x86-64 | Skia | ✅ Working | 15/15 tests pass; DirectWrite fonts |

---

## macOS (arm64)

### Prerequisites

```sh
brew install fontconfig ninja
```

### Skia

```sh
cmake -S . -B build-skia \
  -DARTIST_SKIA=ON \
  -DARTIST_BUILD_EXAMPLES=ON \
  -DARTIST_BUILD_TESTS=ON \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=lib/external/vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_TARGET_TRIPLET=arm64-osx \
  -G Ninja
cmake --build build-skia
ctest --test-dir build-skia --output-on-failure
```

### Quartz2D

```sh
cmake -S . -B build-quartz \
  -DARTIST_BUILD_EXAMPLES=ON \
  -DARTIST_BUILD_TESTS=ON \
  -DCMAKE_BUILD_TYPE=Release \
  -G Ninja
cmake --build build-quartz
ctest --test-dir build-quartz --output-on-failure
```

### Cairo

```sh
brew install cairo fontconfig freetype harfbuzz
cmake -S . -B build-cairo \
  -DARTIST_CAIRO=ON \
  -DARTIST_BUILD_EXAMPLES=ON \
  -DARTIST_BUILD_TESTS=ON \
  -DCMAKE_BUILD_TYPE=Release \
  -G Ninja
cmake --build build-cairo
ctest --test-dir build-cairo --output-on-failure
```

---

## Linux (x86-64) — Ubuntu 24.04

### Prerequisites

```sh
sudo apt install -y \
  libgtk-3-dev \
  libgtkglext1-dev \
  libgl1-mesa-dev \
  libglu1-mesa-dev \
  libegl1-mesa-dev \
  libharfbuzz-dev \
  libfontconfig1-dev \
  libfreetype6-dev \
  x11proto-dev \
  libx11-dev \
  libxext-dev \
  shared-mime-info \
  ninja-build \
  cmake \
  g++ \
  curl zip unzip tar \
  autoconf \
  autoconf-archive \
  automake \
  libtool \
  pkg-config
```

### Skia

vcpkg's toolchain overwrites `PKG_CONFIG_PATH`, hiding system GTK from
cmake's `pkg_check_modules`.  A thin wrapper toolchain is required:

```sh
cat > toolchain-linux.cmake << 'EOF'
include("${CMAKE_CURRENT_LIST_DIR}/lib/external/vcpkg/scripts/buildsystems/vcpkg.cmake")
set(ENV{PKG_CONFIG_PATH} "$ENV{PKG_CONFIG_PATH}:/usr/lib/x86_64-linux-gnu/pkgconfig:/usr/share/pkgconfig")
EOF
```

> **arm64**: replace `x86_64-linux-gnu` with `aarch64-linux-gnu`.

```sh
lib/external/vcpkg/bootstrap-vcpkg.sh -disableMetrics

cmake -S . -B build-skia \
  -DARTIST_SKIA=ON \
  -DARTIST_BUILD_EXAMPLES=ON \
  -DARTIST_BUILD_TESTS=ON \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=toolchain-linux.cmake \
  -DVCPKG_TARGET_TRIPLET=x64-linux \
  -G Ninja \
  -DCMAKE_MAKE_PROGRAM=/usr/bin/ninja

cmake --build build-skia
ctest --test-dir build-skia --output-on-failure
```

> **arm64**: use `-DVCPKG_TARGET_TRIPLET=arm64-linux`.

#### Known issues — Linux harfbuzz conflict

When building the **window host examples** (not headless tests), the vcpkg
static harfbuzz (14.x) and the system dynamic harfbuzz (loaded by GTK) both
end up in the same process.  ELF symbol interposition routes GTK/Pango calls
to the static 14.x copy with 8.x data structures → segfault in
`hb_ot_tags_from_script_and_language`.

**Workaround applied**: `lib/CMakeLists.txt` links against the system
harfbuzz (via `pkg_check_modules`) instead of the vcpkg static harfbuzz for
Linux Skia builds.

**Ubuntu 24.04 arm64**: additionally has a harfbuzz 8.3.0 bug that crashes
even plain GTK apps.  Requires Ubuntu 25.10+ (harfbuzz 10.2.0+).

---

## Linux (arm64) — Ubuntu 26.04 LTS

Same as x86-64 above but:
- `pkgconfig` path: `/usr/lib/aarch64-linux-gnu/pkgconfig`
- vcpkg triplet: `arm64-linux`
- Minimum harfbuzz: 10.2.0 (Ubuntu 24.04's 8.3.0 has a GTK crash bug on arm64)
- `libgtkglext1-dev` dropped in 26.04 — not needed (Skia uses GtkGLArea from GTK3)
- Mesa EGL warnings at startup are harmless (Zink/Vulkan fallback to llvmpipe)
- Verified: all examples run correctly, ~2994 fps on typo example (llvmpipe)

---

## Windows (x86-64) — Windows 10/11

> **Status**: lib/tests CI in progress.  Win32+WGL window host not yet written.

### Prerequisites

- Visual Studio 2022 with C++ workload (Community edition is fine)
- CMake (bundled with VS or standalone in `C:\Program Files\CMake`)
- Ninja. If `winget`/`choco` are unavailable, download the binary directly:
  ```powershell
  New-Item -Force -Path 'C:\tools' -ItemType Directory
  Invoke-WebRequest -Uri 'https://github.com/ninja-build/ninja/releases/download/v1.12.1/ninja-win.zip' -OutFile 'C:\tools\ninja.zip'
  Expand-Archive -Path 'C:\tools\ninja.zip' -DestinationPath 'C:\tools\ninja' -Force
  ```
  Then pass `-DCMAKE_MAKE_PROGRAM=C:/tools/ninja/ninja.exe`.
- The configure must run inside a VS x64 dev environment. Either use the
  "x64 Native Tools Command Prompt", or prefix the cmake call with:
  `"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat" &&`

### Skia

```bat
lib\external\vcpkg\bootstrap-vcpkg.bat -disableMetrics

cmake -S . -B build -G Ninja ^
  -DARTIST_SKIA=ON ^
  -DARTIST_BUILD_EXAMPLES=OFF ^
  -DARTIST_BUILD_TESTS=ON ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_TOOLCHAIN_FILE=lib/external/vcpkg/scripts/buildsystems/vcpkg.cmake

cmake --build build
ctest --test-dir build --output-on-failure
```

### Known issues

- `SkFontMgr_win_gdi.h` not installed by vcpkg — use DirectWrite instead
  (`SkFontMgr_win_dw.h` / `SkFontMgr_New_DirectWrite()`). Fixed in
  `lib/impl/skia/font.cpp`.
- `SkHdrMetadata.h` requires C++20 (designated initialisers). Fixed by
  bumping `CMAKE_CXX_STANDARD` to 20.
- `SkPathBuilder` undefined in `path.hpp` Windows inline — fixed by adding
  `#include <SkPathBuilder.h>`.
- **Stale vcpkg lock**: if `cmake` hangs printing `vcpkg-running.lock: note:
  waiting to take filesystem lock...` repeatedly, an orphaned `vcpkg.exe` from
  a prior session is holding the lock. Fix: `taskkill /F /IM vcpkg.exe`, wipe
  the partial `build-skia` dir, and re-run configure.
- **Directory junctions / cross-drive paths**: do NOT build through a path
  that crosses drive letters via a junction (e.g. `C:\dev\...` junctioned to
  `D:\...`). The vcpkg harfbuzz meson install step calls Python
  `os.path.relpath`, which throws `ValueError: path is on mount 'C:', start on
  mount 'D:'` when the build dir and resolved source land on different drives.
  Fix: build from the real physical path on a single drive
  (`Get-Item <path> | Select Target` to resolve the junction).

---

## vcpkg binary cache (CI)

CI uses GitHub Actions cache for vcpkg binaries:

```yaml
env:
  VCPKG_BINARY_SOURCES: "clear;x-gha,readwrite"
```

The cache is populated on the **first successful build**.  If a run is
cancelled mid-build the cache is not written and the next run rebuilds from
source (~15–20 min for Skia).

---

## Golden image update procedure

When a renderer version bump produces visually-identical but pixel-different
output:

1. Download the `test-results-*` artifact from the CI run.
2. Visually inspect the PNG — confirm it matches the expected output.
3. Copy to `test/<platform>_golden/<backend>/`.
4. Commit and push.  Never lower test thresholds to paper over a regression.

### Confirming a golden delta is not a regression

Non-text scenes (shapes, gradients, composite ops, images) render
**byte-identically across platforms** for the same Skia version + backend.
So to prove a failing golden is merely stale (not a regression), diff the new
render against another platform's *current* golden for the same scene:

```sh
python3 - <<'PY'
from PIL import Image
a=Image.open('build-skia/test/results/shapes_and_images.png').convert('RGB')
b=Image.open('test/linux_golden/skia/shapes_and_images.png').convert('RGB')
W,H=a.size; pa=a.load(); pb=b.load(); c=0
for y in range(H):
  for x in range(W):
    if sum(abs(p-q) for p,q in zip(pa[x,y],pb[x,y]))>30: c+=1
print('diff>30:', c, '(%.2f%%)'%(100*c/(W*H)))
PY
```

0% diff ⇒ the render matches the other platform's current golden ⇒ the failing
golden is stale, regenerate it.  **Text scenes are the exception**: glyph
rasterization differs per platform (DirectWrite vs FreeType vs CoreText), so
text goldens legitimately differ between platforms and must each be kept.

### IMPORTANT: goldens are copied into the build dir at configure time

`test/CMakeLists.txt` copies `*_golden/` into the build tree at configure
time, and the test reads them from there.  After editing a source golden you
must refresh the build-dir copy — either re-run cmake configure, or copy the
file directly into `build-*/test/<platform>_golden/<backend>/`.  Re-running
`ctest` alone will keep comparing against the stale build-dir copy.
