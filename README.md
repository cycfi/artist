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
| Cairo      | macOS, Linux    | `ARTIST_CAIRO=ON`     | off     |

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

### Cairo: supported platforms and known limitations

**Supported:** offscreen image surfaces on macOS and Linux.

**Not yet supported:** live window rendering — there is no X11/XCB,
Wayland, or macOS cairo-quartz window host. Cairo is currently usable
for offscreen rendering and the visual test suite only.

**Known limitations:**

- `shadow_style` is a no-op — Cairo has no native drop-shadow. The
  Skia `SkImageFilters::DropShadow` equivalent requires an explicit
  compositing step not yet implemented.
- `darker` composite op maps to `CAIRO_OPERATOR_DARKEN` (channel-min),
  not the W3C PlusDarker formula `max(0, Cs+Cd-1)`. No exact Cairo
  equivalent exists.
- Text rendering uses FreeType/Fontconfig. HarfBuzz shaping, OpenType
  ligatures, complex scripts, and bidi are not implemented.
- `text_layout::text(…)` re-creates the layout impl but does not
  preserve the original `font_descr`; font selection may be approximate
  after a text update.
- `image::image(uint8_t const*, pixel_format, extent)` (`make_image`)
  is not yet implemented and throws at runtime.
- `path::operator==` compares by pointer identity only; deep path
  equality is not implemented.
- Cairo pixel format for `image::pixels()` is premultiplied BGRA
  (`CAIRO_FORMAT_ARGB32` on little-endian), not straight-alpha RGBA.
  Callers that read or write raw pixels must account for this.

**CI:** the `build-cairo` CI job builds on Ubuntu/GCC with
`ARTIST_CAIRO=ON`. Visual tests are not run in CI because Linux Cairo
golden images have not been generated yet. To enable visual tests,
generate `test/linux_golden/cairo/*.png` on a representative Linux
environment, review them, and commit.

## News

* 30 May 2026: Cairo backend fully integrated — supports macOS, Linux, and Windows; CI build job added for Ubuntu.

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
