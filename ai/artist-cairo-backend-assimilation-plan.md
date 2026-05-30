# Artist Cairo Backend Assimilation Plan

## Goal

Bring the old Artist Cairo backend into the current `master` branch as a first-class optional backend, alongside Skia and Apple Quartz2D.

This is a staged port. Do not attempt to finish everything in one pass.

The main goal is not to preserve the old Cairo branch API. The main goal is to make Cairo conform to the current Artist architecture, while allowing carefully justified API evolution where Cairo exposes real abstraction issues.

## Core principle

The current `master` branch is the behavioral and architectural source of truth.

The old Cairo backend branch is reference material only.

Correct direction:

```text
current Artist master API
    ↓
Cairo backend implementation
    ↓
eventual backend parity with Skia and Quartz2D
```

Wrong direction:

```text
old Cairo backend API
    ↓
force current Artist master to fit it
```

## Important context

The Cairo backend may have three kinds of issues:

1. It may use an older Artist API.
2. Some methods may be missing or incomplete.
3. Cairo itself may expose weaknesses in the current Artist backend abstraction.

The first two should usually be solved by adapting Cairo.

The third may require careful API changes, but only after analysis.

## Hard constraints

Do not rewrite Artist.

Do not blindly merge the old Cairo branch.

Do not change public APIs casually.

Do not change public APIs merely to match the old Cairo branch.

Do not break Skia.

Do not break Quartz2D.

Do not make Cairo mandatory.

Do not remove existing backends.

Do not do broad formatting-only changes.

Do not silently fake unsupported rendering behavior.

Keep each stage small, focused, and reviewable.

## Backend targets

Artist should eventually support these backend families:

```text
Skia
Quartz2D
Cairo
```

Cairo must coexist with Skia and Quartz2D. It must not replace them.

Cairo should be selectable through CMake in the same style as the existing backend options.

Conceptually:

```sh
cmake -S . -B build-skia   -DARTIST_SKIA=ON   -DARTIST_CAIRO=OFF
cmake -S . -B build-cairo  -DARTIST_CAIRO=ON  -DARTIST_SKIA=OFF
cmake -S . -B build-quartz -DARTIST_QUARTZ=ON -DARTIST_SKIA=OFF
```

Use the actual option names and style already present in the repository.

## Preferred integration strategy

Do not start with a full merge.

Use this workflow:

1. Create a clean working branch from current `master`.
2. Inspect the old Cairo branch.
3. Identify Cairo-specific files, build-system changes, and abstractions.
4. Copy or cherry-pick only useful Cairo backend implementation pieces.
5. Rewrite or adapt those pieces to match current `master`.
6. Use the current Skia and Quartz2D backends as references.
7. Document all remaining gaps.

Use a full merge only if the diff is small and clearly limited to Cairo-related files. If the merge touches unrelated public APIs, examples, formatting, or shared abstractions unnecessarily, abort the merge and proceed selectively.

## API change policy

The current `master` API is the starting point, but it is not sacred.

Cairo integration may reveal that some APIs are too Skia-specific, Quartz-specific, platform-specific, or otherwise awkward for a third backend.

API changes are allowed only when they improve Artist as a backend-neutral graphics abstraction.

Do not change shared or public APIs just because the old Cairo code is easier to port that way.

### API mismatch decision order

When an API mismatch appears, try solutions in this order:

```text
1. Adapt Cairo to the current API.
2. Add a Cairo-specific backend adapter.
3. Add a private/shared backend helper.
4. Add a backward-compatible API extension.
5. Change public API only if the current abstraction is genuinely wrong.
```

### API change decision questions

Before changing any public or shared Artist API, answer these in `ai/handoff.md`:

```md
## API change analysis

### Proposed change

### Current API assumption

### Why Cairo has trouble with it

### How Skia currently handles it

### How Quartz2D currently handles it

### Can Cairo adapt locally?

### Why local adaptation is insufficient

### Does the change improve all backends?

### Impact on Skia

### Impact on Quartz2D

### Impact on Cairo

### Impact on Elements

### Source compatibility impact

### Migration notes

### Decision
```

### Backend-neutrality test

A proposed API change should pass this test:

```text
Can this API be implemented naturally by Skia, Quartz2D, and Cairo without forcing one backend to pretend to be another?
```

If not, the abstraction may need to be split, narrowed, adapted, or feature-gated.

## Handling unimplemented Cairo methods

Some Cairo backend methods are expected to be missing or incomplete.

Do not try to complete everything immediately.

For each missing or incomplete method:

1. Compare the equivalent Skia implementation.
2. Compare the equivalent Quartz2D implementation, if available.
3. Determine the expected behavior from the current Artist API.
4. Implement only if the Cairo mapping is clear and low-risk.
5. If unclear, leave a narrow TODO.
6. Document it in `ai/handoff.md`.

Do not silently ignore methods that affect rendering correctness.

Acceptable TODO style:

```cpp
// TODO(cairo): Implement using current Artist backend contract.
// See Skia/Quartz2D implementation for expected behavior.
```

If the project uses exceptions and the method must not silently succeed:

```cpp
throw std::runtime_error{"artist cairo backend: <method_name> is not implemented"};
```

If the project uses assertions instead:

```cpp
CYCFI_ASSERT(false && "artist cairo backend: <method_name> is not implemented");
```

Use the existing project style. Do not introduce a new error-handling convention unnecessarily.

For approximate implementations:

```cpp
// TODO(cairo): Approximate implementation. Needs visual parity check.
```

Every incomplete or approximate method must be listed in the handoff.

## Stage 0: Reconnaissance and API pressure analysis

### Goal

Understand the size and shape of the port before changing code.

### Tasks

1. Inspect current `master` backend architecture.
2. Inspect the old Cairo branch.
3. Identify Cairo-specific files.
4. Identify old Cairo build-system changes.
5. Compare old Cairo assumptions against current `master`.
6. List API mismatches.
7. List missing or incomplete Cairo methods.
8. Identify areas where Cairo may put pressure on the current Artist API.
9. Propose a staged implementation plan.

### Commands

```sh
git status
git branch -a
git log --oneline --decorate --graph --all --max-count=100
git branch -a | grep -i cairo
```

Inspect current master:

```sh
git checkout master
git pull --ff-only
find . -maxdepth 3 -type f | sort
```

Inspect the old Cairo branch:

```sh
git checkout <cairo-branch-name>
git diff --stat master...<cairo-branch-name>
git log --oneline --decorate --left-right --cherry-pick master...<cairo-branch-name>
```

Return to master before making any branch:

```sh
git checkout master
```

### Deliverable

Write to `ai/handoff.md`:

```md
# Artist Cairo backend reconnaissance and API pressure analysis

## Current master backend structure

## Existing backend selection model

## Skia backend structure

## Quartz2D backend structure

## Old Cairo branch name

## Old Cairo backend files and changes

## Cairo-specific dependencies

## API mismatches against current master

## Missing or incomplete Cairo methods

## Possible API pressure points

For each item:

- Current API:
- Why Cairo has trouble with it:
- How Skia handles it:
- How Quartz2D handles it:
- Option A, adapt Cairo locally:
- Option B, change shared Artist API:
- Recommendation:

## Risk areas

- text:
- fonts:
- paths:
- clipping:
- transforms:
- gradients:
- images:
- offscreen surfaces:
- pixel formats:
- HiDPI:
- platform integration:
- CMake:
- CI:

## Recommended staged plan

## Suggested next Claude Code task
```

### Success criteria

- No functional code changes.
- Cairo branch is understood.
- API pressure points are documented.
- Next stage is clearly scoped.

## Stage 1: Build-system integration

### Goal

Make Cairo selectable as an optional backend in CMake without disturbing Skia or Quartz2D.

### Reconnaissance findings that affect this stage

The `cairo_backport` branch already has working CMake changes. Do not blindly merge them.
Take only the following pieces and adapt them:

**Take from `cairo_backport`:**

- Root `CMakeLists.txt`: add `ARTIST_CAIRO OFF` option; add mutual-exclusion logic
  (`if (ARTIST_CAIRO) set(ARTIST_QUARTZ_2D OFF) set(ARTIST_SKIA OFF) endif()`).
- `lib/CMakeLists.txt`: add Cairo `elseif` branch populating `ARTIST_IMPL`;
  add `pkg_check_modules(cairo REQUIRED IMPORTED_TARGET cairo)` and
  `target_link_libraries(artist PUBLIC PkgConfig::cairo)`;
  add `ARTIST_CAIRO` compile definition in `target_compile_definitions`.

**Do NOT take from `cairo_backport`:**

- `add_library(artist)` — the branch removed `STATIC`. This is an unrelated change.
  Keep `add_library(artist STATIC)`.
- Any changes to public headers (`canvas.hpp`, `image.hpp`, `path.hpp`). Those belong in Stage 2.
- The `stb_image.h` placement. That belongs in Stage 2.
- The examples `CMakeLists.txt` Cairo host selection. That belongs in Stage 3.
- `lib/infra` submodule pointer change. Do not update unless independently needed.

### Tasks

1. Create a clean working branch from `artist_2024_dev` (current working branch).
2. Add `ARTIST_CAIRO OFF` option to root `CMakeLists.txt` using the existing style.
3. Add mutual-exclusion logic: enabling Cairo forces Skia and Quartz2D off.
4. Add Cairo dependency discovery in `lib/CMakeLists.txt` using `pkg_check_modules`.
5. Add Cairo source-file selection (`ARTIST_IMPL`) in `lib/CMakeLists.txt`.
6. Add `ARTIST_CAIRO` compile definition.
7. Create a minimal placeholder `lib/impl/cairo/canvas.cpp` (just includes and an empty namespace block) so CMake can select and compile a Cairo source file without failing at link.
8. Verify existing Skia and Quartz2D builds are unaffected.

### Branch

```sh
git checkout artist_2024_dev
git checkout -b artist-cairo-stage-1-build-system
```

### Expected backend defaults

```text
Apple:
  Quartz2D ON by default
  Skia OFF by default
  Cairo OFF by default

Non-Apple:
  Skia ON by default
  Quartz2D OFF
  Cairo OFF by default
```

Cairo is opt-in everywhere. Enable it explicitly:

```sh
cmake -S . -B build-cairo -DARTIST_CAIRO=ON -DARTIST_SKIA=OFF
```

### Cairo dependency discovery

The repo uses `pkg-config` style for Skia dependencies on Linux. Match that:

```cmake
elseif (ARTIST_CAIRO AND NOT MSVC)
   pkg_check_modules(cairo REQUIRED IMPORTED_TARGET cairo)
   target_link_libraries(artist PUBLIC PkgConfig::cairo)
endif()
```

Windows Cairo support is out of scope for now. Do not add MSVC Cairo handling yet.

### Success criteria

- `cmake -S . -B build-cairo -DARTIST_CAIRO=ON` finds the Cairo library and selects sources.
- Existing Skia configuration still works.
- Quartz2D configuration still works on Apple.
- `add_library(artist STATIC)` is unchanged.
- Cairo will fail at compile/link (placeholder only). That is acceptable if documented.

### Handoff update

```md
# Artist Cairo backend handoff

## Current stage

Stage 1: Build-system integration

## What changed

## Files touched

## Backend options

## Cairo dependency discovery

## Build commands tried

## Build results

## Known failures

## Recommended next task
```

## Stage 2: Compile-level Cairo API adaptation

### Goal

Make the Cairo backend compile against the current Artist `master` API.

This stage is about API conformance and compilation, not visual parity.

### Reconnaissance findings that affect this stage

The `cairo_backport` files have several problems that must be fixed, not just ported:

**`image.hpp` — do not port the branch change as-is.**
The branch replaces `class image_impl;` with `using image_impl = cairo_surface_t;`
and pulls `<cairo.h>` into a public header. This leaks internal types.
Correct approach: keep `class image_impl;` in `image.hpp`. In the Cairo backend
define `image_impl` privately as a struct holding `cairo_surface_t*`, matching
the pattern the Skia and Quartz2D backends use for opaque types.

**`path.hpp` — `path_impl = void` is not a real solution.**
The branch uses `using path_impl = void;` because Cairo has no reusable path object.
The correct approach: use `cairo_path_t*` as `path_impl`. Serialize paths via a
scratch `cairo_t` context (`cairo_copy_path`) when a `path` object is built, and
replay via `cairo_append_path` in `canvas::add_path`. This is cumbersome but
it is honest and avoids leaving `path` completely broken.
Document this approach and its overhead.

**`canvas.hpp` — the `ARTIST_CAIRO` forward declaration is acceptable but noisy.**
The branch adds `extern "C" { typedef struct _cairo cairo_t; }` in a shared block.
That is fine. Keep it.

**`stb_image.h` — wrong location.**
Move it from `lib/include/artist/detail/` to `lib/impl/cairo/` (private).
It is an implementation detail; it must not be in the public include tree.

**Gradient pattern leak.**
In `canvas.cpp`, `fill_style(linear_gradient)` and related methods create a
`cairo_pattern_t*` via `cairo_pattern_create_linear/radial` but never call
`cairo_pattern_destroy`. Fix this. Use a `std::function` capture with a
`shared_ptr<cairo_pattern_t>` and a custom deleter, or call `cairo_pattern_destroy`
immediately after `cairo_set_source` (Cairo takes a reference internally).

**`add_library(artist STATIC)` — do not change.**
The branch removed `STATIC`. Do not carry that forward.

**`non_copyable` base on `image` — do not port.**
`image` already has deleted copy constructor and copy assignment. Adding
`non_copyable` as a base is redundant and unrelated to the port.

### Tasks

1. Fix `image.hpp`: keep `class image_impl;` opaque. Define `image_impl` as a
   private struct in `lib/impl/cairo/image.cpp`.
2. Fix `path.hpp`: add `ARTIST_CAIRO` branch using `cairo_path_t*` as `path_impl`.
3. Add `ARTIST_CAIRO` forward declaration block to `canvas.hpp`.
4. Move `stb_image.h` to `lib/impl/cairo/` and adjust the include in `image.cpp`.
5. Bring in `canvas.cpp` from `cairo_backport`. Adapt to current master method list.
   Add explicit `TODO(cairo)` stubs for all missing methods (see list in `ai/handoff.md`).
6. Bring in `path.cpp`. Implement using `cairo_path_t*`.
7. Bring in `font.cpp`, `text_layout.cpp`. Leave as explicit stubs with `TODO(cairo)`.
8. Bring in `image.cpp`. Fix `image_impl` to use private struct. Fix gradient leak.
   Leave `pixels()`, `save_png()`, `bitmap_size()`, `offscreen_image::context()` as
   explicit stubs.
9. Use Skia and Quartz2D as references for every method. Do not guess behavior.
10. Avoid public API changes unless absolutely necessary and documented in `ai/handoff.md`.

### Methods missing from `cairo_backport` canvas.cpp that must be stubbed

All of these are present in the current `canvas.hpp` API and absent or empty in the
Cairo branch. Each must get an explicit `TODO(cairo)` stub — not a silent no-op:

```
pre_scale(float)
skew(double, double)
device_to_user(point)
user_to_device(point)
transform()                   — getter
transform(affine_transform)   — setter
transform(a,b,c,d,tx,ty)
clip(path const&)
clip_extent()
point_in_path(point)
fill_extent()
add_circle(circle const&)
clear_rect(rect const&)
fill_rule(path::fill_rule_enum)
fill_rect(rect const&)        — may be composable from primitives
fill_round_rect(…)            — may be composable from primitives
stroke_rect(…)                — may be composable from primitives
stroke_round_rect(…)          — may be composable from primitives
add_path(path const&)
shadow_style(…)               — known limitation; document explicitly
global_composite_operation(…) — partial; document which ops Cairo supports
```

Note: `fill_rect`, `stroke_rect`, and the round-rect variants are inline convenience
wrappers in `canvas_impl.hpp` that call primitives. Many may work automatically once
the primitives are implemented.

### Success criteria

- Cairo backend compiles.
- Existing Skia backend still compiles.
- Existing Quartz2D backend still compiles on Apple.
- No unimplemented Cairo method silently succeeds.
- All incomplete and approximate methods are documented.
- Any API change is explicitly justified.

### Recommended prompt for this stage

```text
Make the Cairo backend compile against the current Artist master backend API.

Do not implement advanced rendering parity yet.

Use Skia and Quartz2D as the reference for current backend behavior.

Adapt Cairo to the current API where possible.

If an API mismatch suggests the shared Artist abstraction may need to evolve, document the issue before changing the API.

Leave explicit TODOs for unsupported Cairo methods.

Do not silently ignore unsupported rendering operations.

Update ai/handoff.md with all incomplete and approximate methods.
```

### Handoff section for incomplete methods

```md
## Cairo incomplete methods

- `method_name`: not implemented; reason; suggested next step
- `method_name`: approximate implementation; needs visual comparison
- `method_name`: blocked by missing text/image/path support
```

## Stage 3: Minimal Cairo rendering smoke test

### Goal

Prove that Cairo can render basic output.

This is still not full parity.

### Focus features

Implement and verify only the most basic drawing operations:

```text
clear
solid color fill
stroke
rectangle
simple path
save/restore
basic transform
basic clipping, if straightforward
```

Avoid deep text, images, gradients, advanced compositing, and platform-native surfaces unless already working.

### Tasks

1. Build one small example with Cairo.
2. Render basic primitives.
3. Compare manually against Skia or Quartz2D.
4. Fix obvious coordinate, color, alpha, and transform errors.
5. Document limitations.

### Success criteria

- One simple Cairo-rendered example runs.
- Output is visually plausible.
- Existing backends still build.
- Known limitations are documented.

### Handoff

```md
## Cairo smoke test

### Example used

### Features exercised

### Result

### Visual differences noticed

### Known limitations

### Recommended next task
```

## Stage 4: Core path and drawing parity

### Goal

Bring basic vector drawing behavior closer to Skia and Quartz2D.

### Focus areas

```text
paths
fill rules
strokes
line width
line caps
line joins
dashes
curves
transforms
clipping
alpha
compositing basics
```

### Tasks

1. Compare current Skia and Quartz2D implementations.
2. Implement Cairo equivalents where direct mapping is clear.
3. Add small tests or examples only where helpful.
4. Avoid introducing golden-image tests yet unless infrastructure already exists.
5. Document any semantic differences.

### Success criteria

- Common path rendering works.
- Stroke and fill behavior are reasonable.
- Transform and clipping behavior are usable.
- Remaining differences are documented.

## Stage 5: Text and font support

### Goal

Bring Cairo text behavior into basic functional parity.

### Reconnaissance findings that affect this stage

The `cairo_backport` branch uses Cairo's toy font API (`cairo_show_text`,
`cairo_text_path`, `cairo_text_extents`, `cairo_scaled_font_extents`).
The toy API is not suitable for production:

- It has no concept of `font_descr` (family, weight, slant, size).
- Font metrics from `cairo_text_extents` are unreliable for anything beyond ASCII.
- It has no glyph-level layout or Unicode shaping.

The Artist `font` class is initialized from a `font_descr`. That must be mapped
to a real Cairo font. The options are:

1. **`cairo_toy_font_face_t`** — simple but unreliable. Acceptable only as a temporary stub.
2. **FreeType + Fontconfig via `cairo_ft_font_face_create_for_pattern`** — the correct
   production path on Linux. Requires `libcairo-dev` to be built with FreeType support
   (it usually is on Linux).
3. **Pango + `pangocairo`** — full shaping and layout, but adds a heavy dependency.
   Out of scope unless the project already uses Pango.

Recommended approach: use FreeType/Fontconfig for font selection and metrics.
Leave `text_layout` stubbed until text rendering is validated, then implement
it using Cairo's glyph API.

Do not use the toy font API for anything that affects font metrics or glyph layout.
The toy API is acceptable only for `fill_text` / `stroke_text` as a placeholder
while the real implementation is developed, with an explicit `TODO(cairo)` comment.

### Focus areas

```text
font selection (font_descr → cairo_font_face_t via FreeType/Fontconfig)
font metrics (ascent, descent, leading)
text rendering (fill_text, stroke_text)
baseline
ascent/descent
text alignment
glyph positioning
fallback behavior
```

### Tasks

1. Inspect Artist's current text/font abstraction.
2. Compare Skia and Quartz2D text handling.
3. Decide whether Cairo can adapt locally or whether the abstraction needs adjustment.
4. Map `font_descr` to a FreeType-backed `cairo_font_face_t`.
5. Implement font metrics using `cairo_font_extents`.
6. Implement basic `fill_text` / `stroke_text`.
7. Leave `text_layout` stubbed with explicit TODO until text rendering is validated.
8. Document differences.

### API caution

Text is the highest-risk area for backend abstraction. Do not change text APIs casually.

If Cairo exposes a text abstraction problem, document it using the API change analysis template before changing shared APIs.

### Success criteria

- Basic text renders.
- Font metrics are reasonable.
- Examples using text run.
- Differences from Skia/Quartz2D are documented.

## Stage 6: Images, pixel formats, and offscreen surfaces

### Goal

Implement image handling and offscreen rendering for Cairo.

### Reconnaissance findings that affect this stage

The `cairo_backport` image implementation has these specific gaps:

- `pixels()` returns `nullptr`. Cairo surfaces expose raw pixel data via
  `cairo_image_surface_get_data()`. The Artist API returns `uint32_t*`.
  Cairo's `CAIRO_FORMAT_ARGB32` on little-endian is BGRA in memory, not RGBA.
  Conversion or documentation is required.
- `bitmap_size()` returns an empty extent. Cairo surfaces have integer pixel
  dimensions that must be scaled by device scale for HiDPI. Implement using
  `cairo_image_surface_get_width/height`.
- `save_png()` is a stub. Cairo provides `cairo_surface_write_to_png`. Implement directly.
- `offscreen_image::context()` returns `nullptr`. Implement by creating a `cairo_t`
  from the backing `cairo_image_surface_t`.
- `stb_image.h` is placed in the public include directory. Move it to `lib/impl/cairo/`
  (this should have been done in Stage 2 — confirm it was).
- The ARGB32 premultiplied alpha format used by Cairo for image surfaces differs from
  the straight-alpha RGBA expected by the Artist `pixel_format` enum. Document this
  clearly. Do not silently swap formats.

### Focus areas

```text
image surfaces
pixel formats (Cairo ARGB32 premultiplied BGRA vs. Artist RGBA)
stride
premultiplied alpha
surface lifetime
offscreen rendering
drawing image surfaces
capture/snapshot behavior
```

### Tasks

1. Map current Artist image/surface abstractions to Cairo.
2. Compare Skia and Quartz2D behavior.
3. Implement `pixels()` with documented format (BGRA premultiplied or converted).
4. Implement `bitmap_size()`.
5. Implement `save_png()` using `cairo_surface_write_to_png`.
6. Implement `offscreen_image::context()` using `cairo_create` on the surface.
7. Implement offscreen surfaces if applicable.
8. Document the pixel format difference explicitly.

### API caution

Pixel formats and ownership/lifetime semantics are medium-to-high-risk API areas. Prefer backend-local conversion before changing public APIs.

### Success criteria

- Image-related examples compile and run.
- Offscreen rendering works or limitations are explicit.
- Pixel-format behavior is documented.

## Stage 7: Gradients, patterns, and advanced compositing

### Goal

Close advanced rendering gaps.

### Reconnaissance findings that affect this stage

The `cairo_backport` canvas.cpp already implements `fill_style(linear_gradient)`,
`fill_style(radial_gradient)`, `stroke_style(linear_gradient)`, and
`stroke_style(radial_gradient)` via `cairo_pattern_create_linear/radial`.

**Gradient pattern memory leak:** The branch creates `cairo_pattern_t*` inside
`std::function` lambdas but never calls `cairo_pattern_destroy`. Fix this before
Stage 7 if not already done in Stage 2. After `cairo_set_source(ctx, pat)`,
Cairo holds a reference, so `cairo_pattern_destroy(pat)` is safe to call
immediately to release the caller's reference.

**`global_composite_operation` coverage gaps:**
Cairo's `cairo_operator_t` covers the Porter-Duff set but is missing or approximate
for these Artist ops: `darker`, `color_dodge`, `color_burn`, `soft_light`,
`hard_light`, `hue`, `saturation`, `color_op`, `luminosity`.
Cairo 1.10+ added `CAIRO_OPERATOR_HSL_*` for the HSL blend modes on some platforms.
Do not silently map unsupported ops to `CAIRO_OPERATOR_OVER`. Use explicit
`TODO(cairo)` stubs or throw for unimplemented ops.

**`shadow_style` is a known limitation:**
Cairo has no native drop-shadow. Do not implement a compositing workaround unless
requested. Document as a known limitation and leave as an explicit no-op with a comment.

### Focus areas

```text
linear gradients (already partially working — fix memory leak)
radial gradients (already partially working — fix memory leak)
patterns
alpha masks
advanced compositing (document gaps)
blend modes (map supported; document unsupported)
complex clipping
nested save/restore interactions
```

### Tasks

1. Confirm gradient pattern memory leak is fixed (should be done in Stage 2).
2. Implement `global_composite_operation` for the Cairo-supported subset.
3. Document which composite ops are unsupported on Cairo and leave explicit stubs.
4. Document `shadow_style` as a known limitation.
5. Add focused smoke examples if needed.
6. Avoid broad rendering-test infrastructure until output stabilizes.

### Success criteria

- Common gradients work.
- Compositing behavior is understood.
- Unsupported features are explicit.
- No silent wrong rendering.

## Stage 8: Platform integration and HiDPI

### Goal

Make Cairo usable in real platform contexts.

### Reconnaissance findings that affect this stage

**Current platform situation:**
The `cairo_backport` branch has one Cairo host: `examples/host/macos/cairo_app.mm`,
which uses `cairo-quartz.h` (Cairo's Quartz backend). This makes Cairo on macOS
depend on both `libcairo` and the Quartz surface extension.

The branch also includes Wayland and X11 commits from master, but there is no
Cairo-specific Linux window host. The primary intended platform for Cairo should
be Linux (X11 or Wayland with an XCB or Xlib Cairo surface).

**HiDPI:**
`canvas::pre_scale(float)` is empty in the Cairo backend. Cairo's equivalent is
`cairo_surface_set_device_scale`. This must be implemented for correct HiDPI rendering.

**Platform recommendation:**
Target Linux as the primary Cairo platform. macOS via `cairo-quartz` is a secondary
target and adds a less common dependency. Do not block Stage 8 on macOS Cairo support.

### Focus areas

```text
window surfaces (Linux X11/XCB or Wayland first)
native handles
HiDPI scale (pre_scale → cairo_surface_set_device_scale)
coordinate mapping
surface resize
platform-specific initialization
Linux desktop integration
macOS cairo-quartz (secondary)
```

### Tasks

1. Determine primary platform target for Cairo (recommend: Linux first).
2. Implement `pre_scale` using `cairo_surface_set_device_scale`.
3. Implement a minimal Linux Cairo window host (X11 XCB surface or Wayland).
4. Document `cairo_app.mm` as a secondary macOS Cairo target.
5. Validate HiDPI scaling on the primary platform.
6. Document supported platforms clearly.

### Success criteria

- Cairo platform target is clear.
- HiDPI behavior is documented.
- Platform limitations are explicit.

## Stage 9: Tests and CI

### Goal

Make the Cairo backend maintainable.

### Tasks

1. Add a Cairo CI build if dependency setup is simple.
2. Add focused compile and smoke tests.
3. Add visual regression tests only after Cairo rendering stabilizes.
4. Avoid fragile golden images too early.
5. Keep Skia and Quartz2D CI unaffected.

### Likely Linux CI dependencies

```sh
sudo apt-get update
sudo apt-get install -y libcairo2-dev pkg-config
```

Use actual package names required by the final implementation.

### Success criteria

- Cairo build is covered by CI, at least on Linux.
- Existing backend CI still passes.
- Smoke tests cover basic Cairo rendering.
- Known limitations remain documented.

## Stage 10: Documentation and cleanup

### Goal

Make Cairo support understandable to future maintainers and users.

### Tasks

1. Update README or backend docs.
2. Document CMake options.
3. Document Cairo dependencies.
4. Document supported platforms.
5. Document known limitations.
6. Remove stale code.
7. Remove broad TODOs and replace with specific TODOs.
8. Ensure comments explain real backend differences, not historical branch baggage.

### Success criteria

- Users can build Artist with Cairo from docs.
- Maintainers know which features remain incomplete.
- No old fork-specific assumptions remain undocumented.

## Suggested Claude Code task sequence

Use short tasks like these.

### Task 1

```text
Do Stage 0 only: reconnaissance and API pressure analysis for integrating the old Cairo backend into current Artist master.

Do not change code.

Inspect current master, the old Cairo branch, existing Skia and Quartz2D backends, and write findings to ai/handoff.md.
```

### Task 2

```text
Do Stage 1 only: add optional CMake build-system integration for Cairo.

Read ai/artist-cairo-backend-assimilation-plan.md Stage 1 section before starting.
Read ai/handoff.md for reconnaissance findings.

Take only the CMake changes from cairo_backport. Do not copy header or impl files yet,
except a minimal placeholder lib/impl/cairo/canvas.cpp so CMake can select sources.

Critical constraints:
- Keep add_library(artist STATIC). The cairo_backport removed STATIC; do not carry that forward.
- Do not change canvas.hpp, image.hpp, or path.hpp in this stage.
- Do not add stb_image.h anywhere in this stage.
- Cairo must be opt-in (ARTIST_CAIRO=OFF by default).
- Must not disturb Skia or Quartz2D defaults or builds.

Update ai/handoff.md.
```

### Task 3

```text
Do Stage 2 only: bring in the Cairo backend files selectively and make them compile against current Artist master API.

Read ai/artist-cairo-backend-assimilation-plan.md Stage 2 section before starting.
Read ai/handoff.md for reconnaissance findings — it lists specific problems in the cairo_backport files that must be fixed, not just ported.

Critical constraints:
- Do not port image.hpp change (cairo_surface_t alias). Keep class image_impl opaque; define it privately in the Cairo backend.
- Do not port path_impl = void. Use cairo_path_t* as path_impl.
- Move stb_image.h to lib/impl/cairo/ (private), not the public include tree.
- Fix the gradient cairo_pattern_t* memory leak.
- Do not port the non_copyable base on image — already covered by deleted copy operations.
- Every missing canvas method must get an explicit TODO(cairo) stub, not a silent no-op.

Do not chase visual parity yet.
Document all incomplete and approximate methods in ai/handoff.md.
```

### Task 4

```text
Do Stage 3 only: make one minimal Cairo rendering smoke test work.

Focus on clear, fill, stroke, simple path, save/restore, and transform.

Do not work on text, images, gradients, or advanced compositing unless required for the smoke test.

Update ai/handoff.md.
```

### Task 5

```text
Do Stage 4 only: improve core path and drawing parity for Cairo.

Compare against Skia and Quartz2D.

Focus on paths, fill rules, strokes, line caps, joins, dashes, clipping, and transforms.

Update ai/handoff.md.
```

### Task 6

```text
Do Stage 5 only: implement basic Cairo text and font support.

Treat text API issues carefully.

If an API change seems necessary, document the API pressure analysis before changing shared APIs.

Update ai/handoff.md.
```

### Task 7

```text
Do Stage 6 only: implement Cairo images, pixel formats, and offscreen surfaces.

Be careful with ownership, stride, premultiplied alpha, and lifetime semantics.

Update ai/handoff.md.
```

### Task 8

```text
Do Stage 7 only: implement Cairo gradients, patterns, and advanced compositing where feasible.

Do not silently ignore unsupported features.

Update ai/handoff.md.
```

### Task 9

```text
Do Stage 8 only: review and improve Cairo platform integration and HiDPI behavior.

Clarify supported platforms.

Update ai/handoff.md.
```

### Task 10

```text
Do Stage 9 and Stage 10 only: add Cairo CI/smoke tests where practical, then update docs.

Keep the diff focused.

Update ai/handoff.md.
```

## Post-Stage-10: Productisation (artist_2026_dev session)

### What was done

After Stage 10, the branch was renamed `artist_2026_dev` and the following
work was completed to bring the backend to a fully CI-tested state:

**Branch and CI hygiene**
- Renamed branch `artist-cairo-backend` → `artist_2026_dev`; force-pushed.
- Removed all `Co-Authored-By: Claude` trailers from commit history via
  `git filter-branch`; force-pushed.
- Upgraded `actions/checkout@v2` → `@v4`.
- Added `-DARTIST_BUILD_EXAMPLES=OFF` to the Cairo CI job (avoids OpenGL/GTK).
- Added missing `windows_golden/skia/` and `linux_golden/skia/` goldens for
  `shapes2.png` and `composite_ops2.png`.

**make_image implemented**
`image::image(uint8_t const*, pixel_format, extent)` is now implemented for
Cairo. Converts gray8, rgb16, rgb32, rgba32 to Cairo premultiplied BGRA
ARGB32. `_pixmap_size()` also implemented (was returning 0).

**Chessboard moved to tests**
`examples/chessboard.cpp` deleted. `Chessboard` test case added to
`test/main.cpp`. This exercises `make_image<pixel_format::rgba32>` as a
regression test. Goldens committed for all platforms.

**Cairo macOS example host fixed**
`examples/host/macos/quartz2d_app.mm` Cairo path now renders into a plain
`cairo_image_surface` (Retina-aware pixel dimensions) then blits via
`CGBitmapContextCreate` + `CGImage` + `CGContextDrawImage` with a CTM flip.
This fixes blank/upside-down rendering for all Cairo macOS examples.

**Cairo CI tests enabled**
- Added `ARTIST_BUILD_TESTS=ON` and a `ctest` step to the Cairo CI job.
- Generated `test/linux_golden/cairo/` goldens by running CI with
  `continue-on-error` + artifact upload, downloading results, reviewing, and
  committing. All 10 test cases pass in Linux Cairo CI.

**Definition of done — verified**

```md
- [x] Cairo is selectable through CMake.
- [x] Cairo builds on the intended platform (macOS + Linux).
- [x] Skia still builds and all tests pass.
- [x] Quartz2D still builds on Apple.
- [x] Basic Cairo rendering works.
- [x] Core paths, strokes, fills, transforms, and clipping work.
- [x] Text support is implemented or clearly documented as limited.
- [x] Image/offscreen support is implemented (make_image now works).
- [x] Gradients/compositing support is implemented or clearly documented.
- [x] Platform and HiDPI behavior are documented.
- [x] CI includes Cairo with full test run.
- [x] README/docs explain how to build with Cairo.
- [x] Remaining backend differences are documented.
```

### Remaining known limitations

| Area | Status |
|------|--------|
| `shadow_style` | No-op |
| `darker` composite op | Approximate (OPERATOR_DARKEN, not W3C PlusDarker) |
| Text shaping | No HarfBuzz, OpenType, bidi |
| `text_layout::text()` | Does not preserve `font_descr` across updates |
| `path::operator==` | Pointer identity only |
| `image::pixels()` | Premultiplied BGRA, not straight-alpha RGBA |
| Cairo window host | macOS examples use CGBitmap blit, not a native Cairo window surface |

### Possible next tasks

- Implement a proper Cairo window surface host.
- Implement `shadow_style` via offscreen compositing.
- Implement HarfBuzz shaping for Cairo text.
- Fix `text_layout::text()` to preserve `font_descr`.
- Fix `path::operator==` for deep equality.

---

## Review checklist for every stage

Before stopping, confirm:

```md
- [ ] Diff is focused.
- [ ] No broad formatting-only changes.
- [ ] Skia was not broken intentionally.
- [ ] Quartz2D was not broken intentionally.
- [ ] Cairo remains optional.
- [ ] Public API changes, if any, are documented and justified.
- [ ] Unimplemented Cairo methods do not silently report success.
- [ ] Approximate implementations are labeled.
- [ ] Build commands attempted are recorded.
- [ ] Test commands attempted are recorded.
- [ ] Known failures are documented.
- [ ] ai/handoff.md is updated.
```

## Definition of done for the full Cairo assimilation

The full Cairo port is done when:

```md
- [ ] Cairo is selectable through CMake.
- [ ] Cairo builds on the intended platform.
- [ ] Skia still builds.
- [ ] Quartz2D still builds on Apple.
- [ ] Basic Cairo rendering works.
- [ ] Core paths, strokes, fills, transforms, and clipping work.
- [ ] Text support is implemented or clearly documented as limited.
- [ ] Image/offscreen support is implemented or clearly documented as limited.
- [ ] Gradients/compositing support is implemented or clearly documented as limited.
- [ ] Platform and HiDPI behavior are documented.
- [ ] CI includes Cairo where practical.
- [ ] README/docs explain how to build with Cairo.
- [ ] Remaining backend differences are documented.
```

## Final rule

Buildable and honest is better than complete but wrong.

The first successful milestone is not “Cairo is perfect.”

The first successful milestone is:

```text
Cairo is integrated, optional, buildable, and its limitations are explicit.
```
