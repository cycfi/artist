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

### Building with Cairo

Cairo is an optional backend, disabled by default. To enable it:

```sh
cmake -S . -B build-cairo -DARTIST_CAIRO=ON
cmake --build build-cairo
```

Enabling `ARTIST_CAIRO` automatically disables `ARTIST_SKIA` and
`ARTIST_QUARTZ_2D`.

#### Linux dependencies

```sh
sudo apt-get install -y \
  libcairo2-dev \
  libfontconfig1-dev \
  libfreetype6-dev \
  pkg-config
```

#### macOS dependencies (Homebrew)

```sh
brew install cairo fontconfig pkg-config
```

#### Windows dependencies

Cairo for Windows can be obtained via [vcpkg](https://vcpkg.io/):

```sh
vcpkg install cairo fontconfig
```

### Cairo: supported platforms and known limitations

**Supported:**
- Offscreen image surfaces on macOS, Linux, and Windows.
- Live window rendering on macOS (`cairo_app.mm`), Linux/GTK (`cairo_app.cpp`),
  and Windows/Win32 (`cairo_app.cpp`).
- Visual test suite with goldens for macOS and Linux.
- CI build and visual tests on Ubuntu/GCC.

**Known limitations:**

- `shadow_style` is a no-op — Cairo has no native drop-shadow; an explicit
  offscreen compositing step is required and not yet implemented.
- `darker` composite op uses `CAIRO_OPERATOR_DARKEN` (channel-min) as a known
  approximation of the W3C PlusDarker formula `max(0, Cs+Cd-1)`. No exact
  Cairo equivalent exists. Quartz-2D is exact; Skia has the same approximation.
- Text rendering uses FreeType/Fontconfig. HarfBuzz shaping, OpenType
  ligatures, complex scripts, and bidi are not implemented.
- `image::pixels()` returns backend-native premultiplied BGRA
  (`CAIRO_FORMAT_ARGB32` on little-endian). See the `image.hpp` header for
  the full per-backend pixel layout contract.

## News

* 30 May 2026: Cairo backend fully integrated — live window hosts on macOS, Linux, and Windows; visual test suite with CI on Ubuntu.

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

*Copyright (c) 2014-2023 Joel de Guzman. All rights reserved.*
*Distributed under the [MIT License](https://opensource.org/licenses/MIT)*
