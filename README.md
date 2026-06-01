# ![Artist-Logo](https://cycfi.github.io/assets/img/artist/logo.png) Artist 2D Canvas Library

![alt Artist Sampler](https://cycfi.github.io/assets/img/artist/sampler.jpg)

## Introduction

The Artist 2D Canvas Library is an abstraction layer with an API inspired by
the HTML5 canvas API. The library presents a lean API modeled after the [HTML
Canvas 2D Context specification](https://www.w3.org/TR/2dcontext/). The API
is a not-so-thin layer above various 2D platform-specific and cross-platform
2D "backend" graphics libraries, such as [Skia](https://skia.org/),
[Quartz-2D](https://apple.co/2SljYHw), and [Cairo](https://cairographics.org/).

The Artist library goes beyond the basic HTML5 canvas API with extensions for
dealing with text layout and mechanisms for text editing, fonts and font
management, path creation and manipulation, and image capture and offscreen
graphics.

## Backends

Artist supports three rendering backends. Only one backend is active per build.

| Backend    | Platform        | CMake option          | Default |
|------------|-----------------|-----------------------|---------|
| Quartz-2D  | macOS           | `ARTIST_QUARTZ_2D=ON` | macOS   |
| Skia       | macOS, Linux, Windows | `ARTIST_SKIA=ON` | Linux, Windows |
| Cairo      | macOS, Linux, Windows | `ARTIST_CAIRO=ON` | off     |

### Building with Skia

Skia is the default backend on Linux and Windows. On macOS, Skia is built via
[vcpkg](https://vcpkg.io/), which is included as a git submodule at
`lib/external/vcpkg/`. Bootstrap it once, then configure normally:

```sh
# One-time bootstrap (already done if you cloned with --recurse-submodules)
lib/external/vcpkg/bootstrap-vcpkg.sh -disableMetrics   # macOS/Linux
lib\external\vcpkg\bootstrap-vcpkg.bat -disableMetrics  # Windows

cmake -S . -B build-skia -DARTIST_SKIA=ON \
  -DCMAKE_TOOLCHAIN_FILE=lib/external/vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DARTIST_BUILD_EXAMPLES=ON -DARTIST_BUILD_TESTS=ON
cmake --build build-skia
```

### Building with Cairo

Cairo is an optional backend, disabled by default. Enabling `ARTIST_CAIRO`
automatically disables `ARTIST_SKIA` and `ARTIST_QUARTZ_2D`.

#### Linux

Install system packages, then configure:

```sh
sudo apt-get install -y \
  libcairo2-dev \
  libfontconfig1-dev \
  libfreetype6-dev \
  libharfbuzz-dev \
  pkg-config

cmake -S . -B build-cairo -DARTIST_CAIRO=ON
cmake --build build-cairo
```

#### macOS (Homebrew)

```sh
brew install cairo fontconfig freetype harfbuzz ninja

cmake -S . -B build-cairo -DARTIST_CAIRO=ON
cmake --build build-cairo
```

#### Windows

Dependencies are managed via the bundled vcpkg submodule — no separate
installation needed. Bootstrap vcpkg once, then pass the toolchain file:

```sh
lib\external\vcpkg\bootstrap-vcpkg.bat -disableMetrics

cmake -S . -B build-cairo -DARTIST_CAIRO=ON ^
  -DCMAKE_TOOLCHAIN_FILE=lib/external/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build build-cairo
```

vcpkg will build cairo, freetype, fontconfig, and harfbuzz from source on the
first run (~15–20 min). Subsequent builds reuse the cached `vcpkg_installed/`
directory.

### Cairo: supported platforms and known limitations

**Supported:**
- Offscreen image surfaces on macOS, Linux, and Windows.
- Live window rendering on macOS (`cairo_app.mm`), Linux/GTK (`cairo_app.cpp`),
  and Windows/Win32 (`cairo_app.cpp`).
- Visual test suite with goldens for macOS, Linux, and Windows.
- CI build and visual tests on Ubuntu/GCC, macOS/Clang, and Windows/MSVC.

**Known limitations:**

- `shadow_style` is implemented via a software Gaussian blur (SIMD-accelerated
  where available). There is no GPU-accelerated path; very large blur radii or
  high-frequency redraws may be CPU-intensive.
- `darker` composite op uses `CAIRO_OPERATOR_DARKEN` (channel-min) as a known
  approximation of the W3C PlusDarker formula `max(0, Cs+Cd-1)`. No exact
  Cairo equivalent exists. Quartz-2D is exact; Skia has the same approximation.
- Text rendering uses HarfBuzz for shaping (OpenType GSUB/GPOS), FreeType for
  rasterization, and Fontconfig for font selection. Standard ligatures (`liga`)
  and kerning are active. Deferred: font fallback, bidirectional text, complex
  script segmentation (Arabic, Indic, etc.), explicit OpenType feature control.
- `image::pixels()` returns backend-native premultiplied BGRA
  (`CAIRO_FORMAT_ARGB32` on little-endian). See the `image.hpp` header for
  the full per-backend pixel layout contract.

## News

* 31 May 2026: Skia backend upgraded to m148 via vcpkg — replaces hand-managed prebuilt binaries; vcpkg added as a git submodule at `lib/external/vcpkg/`. Cairo Windows support wired up through the same vcpkg submodule. CI now runs the Cairo backend on Ubuntu, macOS, and Windows.
* 31 May 2026: Cairo backend gains HarfBuzz text shaping — OpenType ligatures, kerning, and correct complex-text advances for `fill_text`, `stroke_text`, `measure_text`, and `text_layout`.
* 30 May 2026: Cairo backend fully integrated — drop-shadow support, live window hosts on macOS, Linux, and Windows; visual test suite with CI on Ubuntu and Windows.

## Documentation

Documentation is work in progress. Stay tuned...

1. [Gallery](http://cycfi.github.io/artist/gallery)
2. [Setup and Installation](http://cycfi.github.io/artist/setup)
3. [Backends](http://cycfi.github.io/artist/backends)
3. [Foundation](http://cycfi.github.io/artist/foundation)

## <a name="jdeguzman"></a>About the Author

Joel got into electronics and programming in the 80s because almost
everything in music, his first love, is becoming electronic and digital.
Since then, he builds his own guitars, effect boxes and synths. He enjoys
playing distortion-laden rock guitar, composes and produces his own music in
his home studio.

Joel de Guzman is the principal architect and engineer at [Cycfi
Research](https://www.cycfi.com/). He is a software engineer specializing in
advanced C++ and an advocate of Open Source. He has authored a number of
highly successful Open Source projects such as
[Boost.Spirit](http://tinyurl.com/ydhotlaf),
[Boost.Phoenix](http://tinyurl.com/y6vkeo5t) and
[Boost.Fusion](http://tinyurl.com/ybn5oq9v). These libraries are all part of
the [Boost Libraries](http://tinyurl.com/jubgged), a well respected,
peer-reviewed, Open Source, collaborative development effort.

-------------------------------------------------------------------------------

*Copyright (c) 2014-2026 Joel de Guzman. All rights reserved.*
*Distributed under the [MIT License](https://opensource.org/licenses/MIT)*
