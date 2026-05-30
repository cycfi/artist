# Artist Cairo Backend Follow-up Plan

## Purpose

This document defines the staged follow-up work after the initial Cairo backend assimilation.

The Cairo backend is now integrated as an optional backend, builds successfully, and passes the current test suite. The remaining work should be handled as small, focused follow-up tasks, not as one large branch.

## Current Cairo backend status

The Cairo backend currently supports:

- Optional CMake backend selection via `ARTIST_CAIRO=ON`.
- Offscreen rendering.
- Canvas drawing primitives.
- Paths, fills, strokes, transforms, clipping, gradients, and compositing.
- Image loading/saving and offscreen image contexts.
- Pixel-buffer image construction.
- FreeType + Fontconfig text rendering.
- Word-wrapped text layout using libunibreak.
- macOS Cairo visual goldens.
- Linux Cairo visual goldens.
- Ubuntu CI build with Cairo visual tests enabled.

## Known remaining limitations

1. Live window rendering is partially supported.
   - macOS: ✓ Done — `quartz2d_app.mm` renders via Cairo image surface + CGImage blit.
   - Linux: No GTK Cairo draw path in `skia_app.cpp` yet (Stage F9).
   - Windows: No Win32 Cairo draw path in `skia_app.cpp` yet (Stage F10).
   - Cairo is currently usable for offscreen rendering, the visual test suite, and macOS live windows.

2. `shadow_style` is a no-op. ⏸ **Deferred** — correct implementation requires offscreen compositing, blur, and integration across all drawing paths. Significant CPU cost; deferred to a dedicated stage.

3. ~~`darker` compositing is approximate on Cairo and Skia.~~ **Done** — Documented as explicit known approximation in `canvas.hpp`, Cairo, and Skia. Quartz2D is correct; Skia/Cairo use channel-min. TODOs removed.

4. Cairo text rendering does not use HarfBuzz. ⏸ **Deferred** — significant text engine work; separate stage.

5. `text_layout::text(...)` does not preserve the original `font_descr`.
   - Updating text may reconstruct the layout with approximate/default font selection.

6. ~~`path::operator==` compares by pointer identity only.~~ **Done** — deep structural equality implemented matching Skia/Quartz2D semantics.

7. ~~`image::pixels()` exposes Cairo-native pixels.~~ **Done** — `image.hpp` now documents the backend-native layout for all three backends; the `mark_dirty` write-back requirement is explicitly noted.

8. ~~Linux Cairo visual tests are not enabled yet.~~ **Done** — Linux Cairo goldens committed and Cairo CI tests are now mandatory.

## Working rules

Use one branch per stage.

Each stage should end with:

- one focused diff
- tests run
- `ai/handoff.md` updated
- known limitations updated
- no unrelated formatting churn
- no broad rewrites

Do not combine platform host work with rendering correctness work.

Do not combine HarfBuzz/text shaping with platform host work.

Do not regenerate unrelated goldens.

Do not change public APIs unless a stage explicitly justifies it.

---

# Stage F0: Follow-up reconnaissance and issue verification ✓ Done

## Goal

Verify the remaining limitation list against the current `artist_2026_dev` branch before making changes.

## Tasks

1. Inspect `ai/handoff.md`.
2. Inspect README/backend documentation.
3. Confirm `make_image` / pixel-buffer image construction is implemented.
4. Confirm the remaining limitations listed in this file are still accurate.
5. Inspect current tests and identify which limitations already have coverage.
6. Write or update a short current-state note.

## Deliverable

Update `ai/handoff.md` with:

```md
# Cairo follow-up reconnaissance

## Verified implemented

## Verified remaining limitations

## Existing test coverage

## Recommended next stage
```

## Success criteria

- Limitation list is accurate.
- No functional code changes.
- Next stage is clearly scoped.

---

# Stage F1: Preserve font_descr in Cairo text_layout::text(...)

## Limitation

`text_layout::text(...)` recreates layout state but does not preserve the original `font_descr`. After replacing text, font selection may become approximate or default.

## Goal

Make Cairo `text_layout::text(...)` preserve the original font description and layout state.

## Scope

Cairo text layout only.

Do not add HarfBuzz.

Do not change public APIs unless absolutely necessary.

Do not change text shaping behavior.

## Tasks

1. Inspect `lib/impl/cairo/text_layout.cpp`.
2. Identify which constructor arguments are needed to reconstruct layout state:
   - original text
   - `font_descr`
   - width / extent
   - alignment
   - justification options
   - any line-break settings
3. Store the original `font_descr` in the Cairo text layout implementation.
4. Store any other layout options needed to recreate equivalent layout state.
5. Update `text_layout::text(...)` to rebuild using preserved state.
6. Add a regression test:
   - create a text layout with a non-default font size or weight
   - call `text(...)` to replace contents
   - verify the resulting metrics/layout still reflect the original font
7. Run Cairo, Quartz2D, and Skia tests where practical.

## Success criteria

- `text_layout::text(...)` preserves font selection.
- Cairo tests pass.
- Quartz2D tests pass.
- Skia tests pass where practical.
- No unrelated text behavior changes.
- `ai/handoff.md` updated.

## Suggested commit message

```text
Preserve Cairo text layout font state
```

---

# Stage F2: Define and implement path equality semantics ✓ Done

## Limitation

`path::operator==` currently compares by pointer identity only. Deep path equality is not implemented.

## Goal

Decide whether `path::operator==` should mean identity or semantic path equality, then implement or document the decision.

## Scope

Start with an audit across all backends.

Do not change only Cairo if the public API implies cross-backend semantic behavior.

## Tasks

1. Inspect current `path::operator==` implementations for Cairo, Skia, and Quartz2D.
2. Determine existing public/API expectation:
   - pointer identity
   - exact structural equality
   - approximate geometric equality
3. If all backends currently use identity and the API does not promise deep equality:
   - document pointer-identity semantics clearly.
4. If semantic equality is expected:
   - implement deep comparison for Cairo.
   - consider whether Skia/Quartz2D need matching fixes.
5. Add tests:
   - same path object compares equal
   - separately built identical paths compare according to chosen semantics
   - different paths compare unequal
   - floating-point tolerance behavior is documented if applicable

## Cairo implementation note

If Cairo stores recorded/copied path data, deep equality can compare:

- path command sequence
- command type
- coordinate count
- coordinate values

Use exact comparison unless existing project conventions require an epsilon.

## Success criteria

- Equality semantics are explicit.
- Tests reflect the chosen semantics.
- Cairo tests pass.
- Other backend tests pass.
- `ai/handoff.md` updated.

## Suggested commit message

```text
Clarify path equality semantics
```

or, if implemented:

```text
Implement Cairo path deep equality
```

---

# Stage F3: Harden raw pixel access contract ✓ Done

## Limitation

`image::pixels()` exposes Cairo premultiplied BGRA memory, not straight-alpha RGBA.

## Goal

Make the raw pixel access contract explicit and safe.

## Scope

Audit and harden pixel access.

Do not redesign image APIs unless a real mismatch is found.

## Tasks

1. Search all call sites of `image::pixels()`.
2. Determine whether any caller assumes straight-alpha RGBA.
3. Confirm Cairo comments clearly state:
   - `CAIRO_FORMAT_ARGB32`
   - premultiplied alpha
   - BGRA memory layout on little-endian
   - need for `cairo_surface_flush` before reading
   - need for `cairo_surface_mark_dirty` after writing
4. Add backend-local conversion helpers if needed:
   - Cairo BGRA premultiplied → straight RGBA
   - straight RGBA → Cairo BGRA premultiplied
5. Add tests for semi-transparent pixels if coverage is missing.
6. Avoid changing public API unless absolutely required.

## Success criteria

- No hidden assumption that Cairo pixels are straight RGBA.
- Pixel read/write behavior is documented.
- Any necessary conversions are implemented.
- Tests pass.
- `ai/handoff.md` updated.

## Suggested commit message

```text
Harden Cairo raw pixel access contract
```

---

# Stage F4: Generate Linux Cairo goldens and enable CI visual tests ✓ Done

## Limitation

Ubuntu CI builds Cairo but does not run Cairo visual tests because Linux Cairo goldens do not exist yet.

## Goal

Generate Linux Cairo visual goldens in a stable Linux environment and enable Cairo tests in CI.

## Scope

Linux Cairo visual tests only.

Do not implement platform hosts.

Do not regenerate macOS goldens.

Do not change rendering behavior unless a Linux-only bug is discovered and is clearly necessary.

## Tasks

1. Use a stable Ubuntu environment matching GitHub Actions as closely as possible.
2. Install dependencies:

```sh
sudo apt-get update
sudo apt-get install -y libcairo2-dev libfontconfig1-dev libfreetype6-dev pkg-config ninja-build
```

3. Configure/build Cairo:

```sh
cmake -S . -B build-cairo -G Ninja -DARTIST_CAIRO=ON
cmake --build build-cairo
```

4. Run the test suite to generate or identify missing Linux Cairo outputs.
5. Review generated PNGs manually.
6. Commit PNG goldens under:

```text
test/linux_golden/cairo/
```

7. Re-run tests against committed goldens.
8. If stable, update `.github/workflows/build_test.yml` so the Cairo CI job runs `ctest`.
9. If unstable due to fonts/environment, document the blocker instead of enabling CI visual tests.

## Font reproducibility caution

Cairo text rendering depends on Fontconfig and installed fonts.

If a specific font package is required for reproducible goldens, install it explicitly in CI.

Do not rely on incidental developer-machine fonts.

## Success criteria

- Linux Cairo goldens committed, or blocker documented.
- Cairo tests pass on Linux using Linux Cairo goldens.
- Cairo CI runs tests if stable.
- Existing CI jobs remain unchanged except the Cairo test step.
- `ai/handoff.md` updated.

## Suggested commit message

```text
Add Linux Cairo visual goldens
```

---

# Stage F5: Define darker / PlusDarker policy ✓ Done

## Limitation

Cairo and Skia do not have a native W3C PlusDarker operator.

Current behavior is approximate.

## Goal

Decide whether to implement exact PlusDarker fallback or keep it explicitly approximate.

## Scope

Compositing semantics only.

Do not mix with shadows, text, or platform hosts.

## Tasks

1. Confirm current Artist API expectation for `darker`.
2. Confirm current backend behavior:
   - Quartz2D
   - Skia
   - Cairo
3. Decide between:
   - exact fallback implementation
   - explicit documented approximation
   - API rename/deprecation, if appropriate
4. If implementing fallback:
   - restrict operation to bounded drawing regions
   - implement per-channel `max(0, Cs + Cd - 1)`
   - handle alpha semantics carefully
   - add visual tests/goldens
5. If keeping approximation:
   - ensure comments and docs do not claim W3C correctness
   - ensure tests encode current expected behavior

## Success criteria

- `darker` behavior is no longer ambiguous.
- Tests and docs match implementation.
- Cairo/Skia limitation is explicit.
- `ai/handoff.md` updated.

## Suggested commit message

```text
Clarify darker composite semantics
```

---

# Stage F6: Implement Cairo shadow_style ⏸ Deferred

## Limitation

`shadow_style` is a no-op on Cairo because Cairo has no native drop-shadow.

## Goal

Implement Cairo shadow rendering through explicit offscreen compositing.

## Scope

Cairo `shadow_style` only.

Do not implement HarfBuzz or platform hosts.

## Important caution

The current API likely sets shadow state before later drawing operations. Correct implementation may require integrating shadow behavior into multiple drawing paths, not just one setter.

## Suggested approach

Start with filled/stroked paths only.

Possible pipeline:

1. Render the source shape into an alpha mask surface.
2. Blur the mask.
3. Tint with shadow color.
4. Offset the shadow.
5. Composite behind the original drawing.
6. Preserve current fill/stroke behavior.

## Tasks

1. Inspect how Skia implements `shadow_style`.
2. Inspect how Quartz2D implements `shadow_style`.
3. Determine whether current Cairo drawing path can intercept affected operations.
4. Implement the smallest correct subset first:
   - filled paths
   - stroked paths
5. Add visual tests/goldens.
6. Document limitations:
   - text shadows
   - image shadows
   - blur quality/performance
   - clipping behavior

## Success criteria

- Cairo `shadow_style` works for the chosen first subset.
- Unsupported cases are explicit.
- Tests/goldens added.
- Cairo tests pass.
- Other backend tests pass.
- `ai/handoff.md` updated.

## Suggested commit message

```text
Implement Cairo path shadows
```

---

# Stage F7: Add HarfBuzz shaping to Cairo text ⏸ Deferred

## Limitation

Cairo text uses FreeType/Fontconfig but not HarfBuzz. OpenType ligatures, complex scripts, and bidi are not implemented.

## Goal

Add proper text shaping for Cairo.

## Scope

Major text engine task.

Do not combine with platform host work.

## Suggested substages

### F7.1: HarfBuzz dependency and shaping audit

- Add HarfBuzz dependency discovery for Cairo builds.
- Inspect font face integration.
- Create a shaping helper prototype.
- No broad layout changes yet.

### F7.2: Latin kerning and ligatures

- Shape simple left-to-right Latin runs.
- Confirm kerning and ligatures.
- Add tests/goldens.

### F7.3: Complex scripts

- Shape scripts requiring contextual glyph selection.
- Add tests only if stable fonts are available.

### F7.4: Bidirectional text

- Add FriBidi, ICU, or another bidi solution if needed.
- Define layout behavior.
- Add tests.

## Success criteria

- Latin kerning/ligatures work.
- Complex scripts are shaped correctly if included.
- Bidi behavior is defined before implementation.
- Existing typography tests still pass.
- `ai/handoff.md` updated.

## Suggested commit message

```text
Add HarfBuzz shaping for Cairo text
```

---

# Stage F8: Add macOS Cairo window host ✓ Done

## Notes

`examples/host/macos/quartz2d_app.mm` already has an `#if defined(ARTIST_CAIRO)` path in
`drawRect:`. It renders into a `cairo_image_surface_t`, scales for Retina, then blits via
`CGBitmapContextCreate` + `CGContextDrawImage`. This avoids the coordinate-system
entanglement of `cairo_quartz_surface_create_for_cg_context`.

No further work required for this stage.

---

# Stage F9: Add Linux Cairo window host ✓ Done

## Limitation

`examples/host/linux/skia_app.cpp` has no Cairo path. The GTK3 `on_draw` signal already
hands a `cairo_t*` to the draw callback — the Elements master host uses it directly.

## Goal

Add an `#if defined(ARTIST_CAIRO)` path to the Linux GTK host, mirroring the Elements
master GTK draw pattern and the existing macOS Cairo path in `quartz2d_app.mm`.

## Implementation model (from Elements master)

```cpp
gboolean on_draw(GtkWidget* widget, cairo_t* cr, gpointer user_data)
{
   // cr is already clipped to the exposed area
   view.draw(cr);   // wrap cr in Artist canvas and draw
   return false;
}
```

GTK passes a ready-made `cairo_t*` in the `draw` signal — no surface creation needed.

## Tasks

1. Add `#if defined(ARTIST_CAIRO)` block to `on_draw` in `skia_app.cpp`.
2. Wrap `cr` in an Artist `canvas` and call `draw(cnv)`.
3. Remove or guard the Skia GL setup for Cairo builds.
4. Confirm resize/redraw — GTK reissues `draw` on resize automatically.
5. Build with `ARTIST_CAIRO=ON` and run examples.

## Success criteria

- Linux Cairo host builds and renders live.
- Cairo offscreen tests still pass.
- Skia build remains unchanged.
- CI remains stable.
- `ai/handoff.md` updated.

## Suggested commit message

```text
Add Linux Cairo window host
```

---

# Stage F10: Add Windows Cairo window host ✓ Done

## Limitation

`examples/host/windows/skia_app.cpp` has no Cairo path. The Elements master Windows host
uses `cairo_win32_surface_create(offscreen_hdc)` in `WM_PAINT`.

## Goal

Add an `#if defined(ARTIST_CAIRO)` path to the Windows host, matching the Elements master
Win32 Cairo draw pattern.

## Implementation model (from Elements master)

```cpp
// In WM_PAINT handler:
cairo_surface_t* surface = cairo_win32_surface_create(info->offscreen_hdc);
cairo_t* context = cairo_create(surface);
cairo_scale(context, scale, scale);
auto cnv = canvas{context};
view->draw(cnv);
cairo_destroy(context);
cairo_surface_destroy(surface);
```

## Tasks

1. Inspect `examples/host/windows/skia_app.cpp` `WM_PAINT` handler.
2. Add `#if defined(ARTIST_CAIRO)` block using `cairo_win32_surface_create`.
3. Handle DPI scaling (multiply by `GetDpiForWindow` / 96.0).
4. Guard Skia GL setup for Cairo builds.
5. Add `cairo-win32` to CMake Windows dependencies.
6. Build and run examples on Windows.

## Success criteria

- Windows Cairo host builds and renders live.
- Cairo offscreen tests still pass.
- Skia build remains unchanged.
- `ai/handoff.md` updated.

## Suggested commit message

```text
Add Windows Cairo window host
```

---

# Suggested branch order

Recommended branch sequence:

```text
artist_2026_dev
  ├─ cairo-text-layout-preserve-font  ✓ done
  ├─ cairo-path-equality              ✓ done
  ├─ cairo-pixel-contract             ✓ done
  ├─ cairo-linux-goldens              ✓ done
  ├─ cairo-darker-policy              ✓ done
  ├─ cairo-macos-host                 ✓ done
  ├─ cairo-linux-host                 ✓ done
  ├─ cairo-windows-host               ✓ done
  ├─ cairo-shadow-style               ⏸ deferred
  └─ cairo-harfbuzz-text              ⏸ deferred
```

## Practical next task

```text
Stage F9: Add Linux Cairo window host
```

## Completed stages

- F0: Reconnaissance — `make_image` confirmed implemented, all limitations verified.
- F1: `text_layout::text()` now preserves the original `font_descr` (commit `b10ce70`).
- F2: `path::operator==` implements deep structural equality — compares `fill_rule`, element types, and coordinates (commit `038347a`).
- F3: `image.hpp` documents backend-native pixel layout for all backends; `mark_dirty` write-back caveat noted (commit `400450e`).
- F4: Linux Cairo goldens committed; Cairo CI tests now mandatory (commit `92130fa`).
- F5: `darker` approximation locked in as explicit policy; `canvas.hpp` enum comment documents Quartz2D-correct vs Skia/Cairo channel-min (commit `1fd7a15`).
- F8: macOS Cairo window host confirmed already implemented in `examples/host/macos/quartz2d_app.mm` via image surface + CGImage blit.
- F9: Linux Cairo window host added in `examples/host/linux/cairo_app.cpp` — GTK drawing area, `on_configure` creates backing surface, `on_draw` blits and calls `draw(cr)`.
- F10: Windows Cairo window host added in `examples/host/windows/cairo_app.cpp` — Win32 offscreen HDC, `cairo_win32_surface_create`, double-buffered via `BitBlt`. CMakeLists updated to select host by backend on all platforms.
