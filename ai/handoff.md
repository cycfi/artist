# Artist Cairo backend handoff

## Current stage

Stage 10: Documentation and cleanup — complete.

---

## Existing CI structure

The repository has one workflow: `.github/workflows/build_test.yml`.

It contains a matrix with three jobs:

| Job name                | OS             | Compiler  | Default backend |
|-------------------------|----------------|-----------|-----------------|
| Windows Latest MSVC     | windows-latest | cl        | Skia (Windows)  |
| Ubuntu Latest GCC       | ubuntu-latest  | gcc/g++   | Skia (Linux)    |
| macOS Latest Clang      | macos-latest   | clang++   | Quartz2D (Apple)|

All three jobs share the same steps: checkout, install deps, cmake configure, cmake build, ctest.

The configure step uses no backend flag; CMake defaults apply per-platform:
- Apple: Quartz2D ON
- Linux/Windows: Skia ON

A new separate job (`build-cairo`) was added outside the matrix (see below).

---

## Cairo dependency requirements

Cairo requires these packages on Ubuntu/Debian Linux:

```sh
sudo apt-get install -y \
  libcairo2-dev \
  libfontconfig1-dev \
  libfreetype6-dev \
  pkg-config \
  ninja-build
```

`libcairo2-dev` provides both the `cairo` and `cairo-ft` pkg-config modules.
The `lib/CMakeLists.txt` requests all three: `cairo`, `cairo-ft`, `fontconfig`.

On macOS (Homebrew):

```sh
brew install cairo fontconfig pkg-config
```

---

## Visual golden considerations

Cairo goldens are currently **macOS-only**, stored under `test/macos_golden/cairo/`.

There are no `test/linux_golden/cairo/` goldens.

The test framework compares rendered output pixel-by-pixel (SSI > 0.985) against
backend/platform-specific golden images. Cairo rendering is font-stack dependent;
macOS Cairo goldens cannot be used on Linux because the font rendering differs.

The test CMakeLists.txt now wires `linux_golden/cairo` as the golden path for UNIX
Cairo builds, so the path is ready when goldens are eventually generated:

```cmake
# test/CMakeLists.txt (UNIX branch)
if (ARTIST_SKIA)
   set(GOLDEN_PATH linux_golden/skia)
elseif (ARTIST_CAIRO)
   set(GOLDEN_PATH linux_golden/cairo)
endif()
```

The directory `test/linux_golden/cairo/` is tracked with a `.gitkeep` file.

Visual tests are **not run in Linux CI** for the Cairo job.

---

## What changed

1. **`.github/workflows/build_test.yml`**: Added a new `build-cairo` job (Ubuntu, GCC)
   that installs Cairo dependencies, configures with `-DARTIST_CAIRO=ON`, and builds.
   No test step — see "CI behavior" section.

2. **`test/CMakeLists.txt`**: Added `elseif (ARTIST_CAIRO)` branch in the UNIX section
   so that `GOLDEN_PATH` is set to `linux_golden/cairo` when Cairo is selected on Linux.
   Previously, Cairo on Linux left `GOLDEN_PATH` empty.

3. **`test/linux_golden/cairo/.gitkeep`**: Created the directory so it is tracked by git
   and the copy step in CMakeLists.txt succeeds. No golden images in it yet.

---

## Files touched

- `.github/workflows/build_test.yml`
- `test/CMakeLists.txt`
- `test/linux_golden/cairo/.gitkeep` (new)
- `ai/handoff.md`

---

## CI behavior

### Existing jobs (unchanged)

| Job                  | Configures with       | Tests run? | Golden dir           |
|----------------------|-----------------------|------------|----------------------|
| Windows Latest MSVC  | default (Skia)        | Yes        | windows_golden/skia  |
| Ubuntu Latest GCC    | default (Skia)        | Yes        | linux_golden/skia    |
| macOS Latest Clang   | default (Quartz2D)    | Yes        | macos_golden/quartz2d|

### New Cairo job

| Job                              | Configures with    | Tests run? | Reason               |
|----------------------------------|--------------------|------------|----------------------|
| Ubuntu Latest GCC (Cairo, build) | ARTIST_CAIRO=ON    | No         | No linux Cairo goldens |

The Cairo CI job verifies that the Cairo backend builds cleanly on Linux with the current
source. It does not run visual tests. A comment in the workflow explains why.

To enable Cairo visual tests in Linux CI, generate `test/linux_golden/cairo/*.png` on a
representative Linux CI environment and commit them to the repository.

---

## Build commands tried

```sh
# Cairo (local macOS)
cmake -S . -B build-cairo -DARTIST_CAIRO=ON
cmake --build build-cairo
ctest --test-dir build-cairo --output-on-failure

# Default backend (Quartz2D, local macOS)
cmake --build build-quartz
ctest --test-dir build-quartz --output-on-failure
```

---

## Test commands tried

```sh
ctest --test-dir build-cairo  --output-on-failure
ctest --test-dir build-quartz --output-on-failure
```

---

## Test results

- **Cairo** (`build-cairo`, macOS): **10/10 PASS** — 0 failures.
- **Quartz2D** (`build-quartz`, macOS): **10/10 PASS** — 0 failures.

---

## Known limitations remaining

- **No Linux Cairo golden images**: Visual tests cannot run for Cairo on Linux CI until
  `test/linux_golden/cairo/*.png` is populated. This requires running Cairo on a Linux CI
  environment, generating results, reviewing them, and committing as goldens.

- **No Cairo platform window host**: Cairo is limited to offscreen image surfaces. No
  X11/XCB, Wayland, or macOS Cairo-Quartz window host exists.

- **`device_to_user` / `user_to_device` on platform hosts**: Cairo's current
  implementation does not factor out an initial platform CTM. If a platform host applies
  a device scale at context creation, Cairo would need the same initial-inverse-CTM
  pattern as Skia and Quartz2D.

- **`shadow_style`**: remains no-op; Cairo has no native drop-shadow.

- **`darker` composite op**: approximate mapping to `CAIRO_OPERATOR_DARKEN`; no exact
  Cairo equivalent for W3C PlusDarker.

- **`image::image(uint8_t const*, pixel_format, extent)`** (`make_image`): still throws.

- **HarfBuzz shaping / bidi / OpenType**: Cairo text limitations from Stage 5 unchanged.

- **`pre_scale`**: Not in the public API; not a Cairo-specific gap.

---

## Scale/platform behavior (from Stage 8)

### `device_to_user` / `user_to_device` behavior

| Backend    | Implementation                                     | Correct for offscreen? |
|------------|----------------------------------------------------|------------------------|
| Quartz2D   | Factors out initial CTM via stored inverse         | Yes                    |
| Skia       | Factors out initial CTM via stored inverse         | Yes                    |
| Cairo      | `cairo_device_to_user` / `cairo_user_to_device`    | Yes (identity initial) |

### `bitmap_size()` behavior

| Backend    | `size()`                               | `bitmap_size()`               |
|------------|----------------------------------------|-------------------------------|
| Quartz2D   | NSImage logical size (points)          | NSBitmapImageRep physical px  |
| Skia       | SkBitmap pixel dimensions              | SkBitmap pixel dimensions     |
| Cairo      | `cairo_image_surface_get_width/height` | Same as `size()`              |

---

## API changes considered

No public Artist API changes in Stage 9.

---

---

## Stage 10 changes

### What changed

1. **`README.md`**: Added "Backends" section documenting:
   - Backend selection table (Quartz-2D / Skia / Cairo) with defaults.
   - Cairo build instructions and CMake option (`ARTIST_CAIRO=ON`).
   - Linux and macOS dependency lists.
   - Supported platforms (offscreen only; no window host yet).
   - All known Cairo limitations (shadow, darker, text shaping, make_image,
     path equality, pixel format).
   - CI status and how to unblock Linux visual tests.
   - Mention of Cairo added to the introduction paragraph.

2. **`lib/impl/cairo/text_layout.cpp`**: Consolidated two duplicate `TODO(cairo)` 
   comments in `text_layout::text()` overloads into single clear TODOs. Removed
   the always-false ternary `_impl->get_font().impl() ? font_descr{} : font_descr{}`
   (both branches returned `font_descr{}`; simplified to a direct `font_descr{}`).

### Files touched

- `README.md`
- `lib/impl/cairo/text_layout.cpp`
- `ai/handoff.md`

### Remaining TODOs in Cairo source

All remaining `TODO(cairo)` markers are specific and intentional:

| File | Location | Description |
|------|----------|-------------|
| `canvas.cpp:408` | `shadow_style` | Drop-shadow via compositing — known limitation |
| `canvas.cpp:431` | `darker` op | W3C PlusDarker not expressible in Cairo |
| `image.cpp:65` | `pixels()` | BE endian verification |
| `image.cpp:90` | `make_image` | Pixel-buffer constructor unimplemented; throws |
| `image.cpp:158` | `_pixmap_size` | Needed by make_image |
| `path.cpp:57` | `operator==` | Deep path equality unimplemented |
| `text_layout.cpp:648` | `text(string_view)` | font_descr not stored in impl |
| `text_layout.cpp:655` | `text(u32string_view)` | Same font_descr gap |

### Known limitations (unchanged from Stage 9)

- Cairo is offscreen-only; no X11/XCB, Wayland, or macOS cairo-quartz window host.
- `shadow_style` is a no-op.
- `darker` op is approximate (OPERATOR_DARKEN, not W3C PlusDarker).
- Text: no HarfBuzz, no OpenType ligatures, no complex scripts, no bidi.
- `make_image` (pixel-buffer constructor) throws.
- `path::operator==` compares by pointer identity.
- `image::pixels()` returns premultiplied BGRA, not straight-alpha RGBA.
- Linux Cairo goldens not generated; visual tests not run in CI.

---

## Recommended next task

To enable Cairo visual tests in Linux CI:
1. Create a Linux CI environment with libcairo2-dev and the same fonts used in testing.
2. Run `cmake -S . -B build-cairo -DARTIST_CAIRO=ON && cmake --build build-cairo`.
3. Run the test binary once to generate result PNGs in `test/results/`.
4. Review each PNG manually.
5. Copy approved PNGs to `test/linux_golden/cairo/`.
6. Commit and push — add a test step to the `build-cairo` CI job.

To add a Cairo window host:
- Linux: implement an XCB (`cairo_xcb_surface_create`) or Wayland host.
- macOS: implement a Quartz-surface host using `cairo_quartz_surface_create_for_cg_context`.
  Requires `cairo` built with Quartz support (`cairo-quartz` pkg-config module).

The full Cairo assimilation checklist from the plan is now satisfied
except for the visual regression test coverage and the platform window host.
