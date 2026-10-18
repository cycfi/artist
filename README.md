# ![Artist-Logo](https://cycfi.github.io/assets/img/artist/logo.png) Artist 2D Canvas Library

[![Build, by platform and backend](https://cycfi.github.io/artist/status/build.svg)](https://github.com/cycfi/artist/actions/workflows/build_test.yml)

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

> **Status:** `master` tracks the latest stable release (currently v1.0.1). The next release, **v1.1**, is developed on `develop`, where the docs stay in sync as changes land. See [NEWS.md](NEWS.md) for what is new in each release.

## Backends

Artist supports five rendering backends. Only one backend is active per build.

| Backend    | Platform        | CMake option          | Default |
|------------|-----------------|-----------------------|---------|
| Quartz-2D  | macOS           | `ARTIST_QUARTZ_2D=ON` | macOS   |
| Direct2D   | Windows         | `ARTIST_DIRECT2D=ON`  | Windows |
| Skia       | macOS, Linux, Windows x64 | `ARTIST_SKIA=ON` | off |
| Cairo      | macOS, Linux, Windows | `ARTIST_CAIRO=ON` | Linux and others |
| Recording  | macOS, Linux, Windows | `ARTIST_RECORDING=ON` | off |

With no backend option set, CMake uses the platform's native backend.
Enabling one backend automatically disables the others. The recording
backend draws nothing: it keeps a journal of the drawing calls, for tests.

[Setup and Installation](https://cycfi.github.io/artist/artist/v1.0/setup.html) covers building with each backend:
the prebuilt Skia and Cairo bundles, building them from source with vcpkg,
and the Cairo backend's
[supported platforms and known limitations](https://cycfi.github.io/artist/artist/v1.0/setup.html#_cairo_supported_platforms_and_known_limitations).

## Documentation

<dl>
  <dt><a href="https://cycfi.github.io/artist/artist/v1.0/index.html">Artist 2D Canvas Library</a></dt>
  <dd><a href="https://cycfi.github.io/artist/artist/v1.0/gallery.html">Gallery</a></dd>
  <dd><a href="https://cycfi.github.io/artist/artist/v1.0/setup.html">Setup and Installation</a></dd>
  <dd><a href="https://cycfi.github.io/artist/artist/v1.0/backends.html">Backends</a></dd>
  <dd><a href="https://cycfi.github.io/artist/artist/v1.0/reference.html">Reference</a></dd>
</dl>

## <a name="jdeguzman"></a>The Author

<img align="right" src="https://github.com/cycfi/elements/blob/assets/images/joel.jpg?raw=true" width="200">

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

## Discord

Join the [Discord channel](https://discord.gg/NJkadSx) to discuss Artist and chat with the developers.

-------------------------------------------------------------------------------

*Copyright (c) 2014-2026 Joel de Guzman. All rights reserved.*
*Distributed under the [MIT License](https://opensource.org/licenses/MIT)*
