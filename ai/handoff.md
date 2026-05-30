# Artist Cairo backend handoff

## Current stage

Stage 7: Cairo gradients, patterns, and compositing — complete.

---

## What changed

### `lib/impl/cairo/canvas.cpp`

1. **`global_composite_operation`: added `default:` throw for unhandled enum values.**
   The switch previously initialised `op = CAIRO_OPERATOR_OVER` before the switch and
   had no `default:` branch.  Any future `composite_op_enum` addition would have
   silently used `OVER`.  The fix removes the pre-initialisation and adds a `default:`
   that throws `std::runtime_error` — matching the project's existing error style and
   the plan's "do not silently map unsupported ops to OVER" policy.

2. **`global_composite_operation`: improved `darker` comment.**
   The prior `// TODO(cairo): Approximate` gave no explanation.  The new comment
   explains that CSS `darker` was a Porter-Duff subtract operation (removed from the
   CSS spec), while Cairo's `CAIRO_OPERATOR_DARKEN` is channel-min blending — a
   semantically different operation.  The mapping is intentionally kept as the closest
   available approximation and is still labelled `// TODO(cairo): approximate`.

3. **`global_composite_operation`: added Cairo ≥ 1.10 grouping comments.**
   The extended blend operators (`MULTIPLY` … `HSL_LUMINOSITY`) were added in
   Cairo 1.10.  Two inline section comments now make this requirement explicit.

---

## Files touched

- `lib/impl/cairo/canvas.cpp`

---

## Gradient behavior verified

| Check | Result |
|-------|--------|
| `fill_style(linear_gradient)` pattern created and destroyed | Pattern created in deferred lambda, destroyed with `cairo_pattern_destroy` immediately after `cairo_set_source`. Cairo holds its own reference. Correct. |
| `fill_style(radial_gradient)` pattern lifetime | Same pattern — create → set_source → destroy. Correct. |
| `stroke_style(linear_gradient)` pattern lifetime | Same. Correct. |
| `stroke_style(radial_gradient)` pattern lifetime | Same. Correct. |
| Color stops (offset, RGBA) | Forwarded via `cairo_pattern_add_color_stop_rgba`. Correct. |
| Linear gradient coordinates | `gr.start`, `gr.end` mapped to `cairo_pattern_create_linear`. Correct. |
| Radial gradient coordinates | `gr.c1/c1_radius`, `gr.c2/c2_radius` mapped to `cairo_pattern_create_radial`. Correct. |

---

## Pattern lifetime behavior

After `cairo_set_source(ctx, pat)`, Cairo increments the pattern's reference count
internally.  The caller's reference is released with `cairo_pattern_destroy(pat)`.
Both calls are safe to pair immediately — the pattern survives until Cairo no longer
needs it.

This is already the pattern used in all four gradient style setters.  No leak exists.

---

## Composite operation behavior

All 24 `composite_op_enum` values are explicitly mapped to a Cairo operator.
No value silently falls through to `CAIRO_OPERATOR_OVER`.

| Artist op | Cairo operator | Notes |
|-----------|---------------|-------|
| `source_over` | `CAIRO_OPERATOR_OVER` | Exact |
| `source_atop` | `CAIRO_OPERATOR_ATOP` | Exact |
| `source_in` | `CAIRO_OPERATOR_IN` | Exact |
| `source_out` | `CAIRO_OPERATOR_OUT` | Exact |
| `destination_over` | `CAIRO_OPERATOR_DEST_OVER` | Exact |
| `destination_atop` | `CAIRO_OPERATOR_DEST_ATOP` | Exact |
| `destination_in` | `CAIRO_OPERATOR_DEST_IN` | Exact |
| `destination_out` | `CAIRO_OPERATOR_DEST_OUT` | Exact |
| `lighter` | `CAIRO_OPERATOR_ADD` | Exact (additive) |
| `darker` | `CAIRO_OPERATOR_DARKEN` | **Approximate** — CSS `darker` (Porter-Duff subtract, now removed from spec) ≠ Cairo DARKEN (channel-min blending). No exact Cairo equivalent. |
| `copy` | `CAIRO_OPERATOR_SOURCE` | Exact |
| `xor_` | `CAIRO_OPERATOR_XOR` | Exact |
| `difference` | `CAIRO_OPERATOR_DIFFERENCE` | Exact; Cairo ≥ 1.10 |
| `exclusion` | `CAIRO_OPERATOR_EXCLUSION` | Exact; Cairo ≥ 1.10 |
| `multiply` | `CAIRO_OPERATOR_MULTIPLY` | Exact; Cairo ≥ 1.10 |
| `screen` | `CAIRO_OPERATOR_SCREEN` | Exact; Cairo ≥ 1.10 |
| `color_dodge` | `CAIRO_OPERATOR_COLOR_DODGE` | Exact; Cairo ≥ 1.10 |
| `color_burn` | `CAIRO_OPERATOR_COLOR_BURN` | Exact; Cairo ≥ 1.10 |
| `soft_light` | `CAIRO_OPERATOR_SOFT_LIGHT` | Exact; Cairo ≥ 1.10 |
| `hard_light` | `CAIRO_OPERATOR_HARD_LIGHT` | Exact; Cairo ≥ 1.10 |
| `hue` | `CAIRO_OPERATOR_HSL_HUE` | Exact; Cairo ≥ 1.10 |
| `saturation` | `CAIRO_OPERATOR_HSL_SATURATION` | Exact; Cairo ≥ 1.10 |
| `color_op` | `CAIRO_OPERATOR_HSL_COLOR` | Exact; Cairo ≥ 1.10 |
| `luminosity` | `CAIRO_OPERATOR_HSL_LUMINOSITY` | Exact; Cairo ≥ 1.10 |

A `default:` branch throws `std::runtime_error` for any future enum value not yet
mapped, preventing silent misbehavior.

---

## Shadow behavior

`shadow_style(point, float, color)` remains an explicit no-op with a comment:

```cpp
// TODO(cairo): Shadow rendering requires compositing with a blurred copy.
// Cairo has no native drop-shadow operation. Known limitation — no-op.
```

No workaround is implemented.

---

## `draw(image)` unbounded-operator clipping

`draw(image, src, dest)` clips to the destination rect before `cairo_paint`.
Cairo's unbounded operators (SOURCE, IN, OUT, XOR, etc.) would otherwise affect
the entire surface outside the intended destination.  The clip guard is in place.

---

## Build commands tried

```sh
cmake --build build-cairo
cmake --build build-quartz
```

---

## Test commands tried

```sh
ctest --test-dir build-cairo  --output-on-failure
ctest --test-dir build-quartz --output-on-failure
```

---

## Test results

- **Cairo** (`build-cairo`): **8/8 PASS** — 0 failures.
- **Quartz2D** (`build-quartz`): **8/8 PASS** — 0 failures.

---

## Bugs fixed

| Bug | Fix |
|-----|-----|
| `global_composite_operation` silently fell back to `CAIRO_OPERATOR_OVER` for any unhandled enum value | Added `default: throw std::runtime_error{...}` |

---

## Known limitations remaining

- **`darker`**: approximate mapping to `CAIRO_OPERATOR_DARKEN` (channel-min blending).
  The original CSS Porter-Duff `darker` (subtract) has no exact Cairo equivalent.

- **`shadow_style`**: no-op; Cairo has no native drop-shadow.

- **Extended blend operators** (`MULTIPLY` and above): require Cairo ≥ 1.10.
  Older Cairo releases will fail to compile (link) against these constants.
  Modern Linux distributions ship Cairo ≥ 1.16.

- **`pixels()` write path**: after direct pixel writes, callers must call
  `cairo_surface_mark_dirty` before the next draw.  The Artist API has no hook for
  this; see Stage 6 handoff.

- **`image::image(uint8_t const*, pixel_format, extent)`** (`make_image`): still
  throws.  Needs straight→premultiplied BGRA conversion when implemented.

- **`bitmap_size()` / HiDPI**: returns logical pixel dimensions; Stage 8 will add
  `cairo_surface_get_device_scale`.

- **`pre_scale` / HiDPI surface scaling**: empty stub; Stage 8.

- **HarfBuzz shaping / bidi / OpenType**: Cairo text limitations from Stage 5 unchanged.

---

## API changes considered

No public Artist API changes were made in Stage 7.

---

## Cross-backend composite op inconsistencies (discovered Stage 7)

The Artist API follows the W3C Compositing and Blending spec.

### `lighter` — Skia bug

W3C `lighter` = Porter-Duff Plus: `min(1, Cs + Cd)` (additive composite).

| Backend | Mapped to | Correct? |
|---------|-----------|---------|
| Cairo | `CAIRO_OPERATOR_ADD` | ✓ |
| Quartz2D | `kCGBlendModePlusLighter` | ✓ |
| **Skia** | `kLighten` — channel-max blend, not Plus | **✗ bug** |

Fix: `kLighten` → `kPlus` in `lib/impl/skia/canvas.cpp`.

### `darker` — Cairo and Skia have no native equivalent

W3C `darker` = CSS2 subtractive: `max(0, Cs + Cd − 1)`. Removed from current CSS spec;
only Quartz2D has a native equivalent (`kCGBlendModePlusDarker`).

| Backend | Mapped to | Correct? |
|---------|-----------|---------|
| Quartz2D | `kCGBlendModePlusDarker` | ✓ |
| **Skia** | `kDarken` — channel-min blend, not subtractive | **✗ unsupported** |
| **Cairo** | `CAIRO_OPERATOR_DARKEN` — channel-min blend, not subtractive | **✗ unsupported** |

Both Skia and Cairo need their `darker` mapping labelled explicitly as unsupported
(not "approximate") with a clear comment explaining W3C PlusDarker semantics and that
no native equivalent exists for the backend.

These are **not** Stage 7 regressions — they are pre-existing across all three backends.
They must be documented and fixed in a dedicated pass, not bundled into Stage 8.

---

## Recommended next task

```text
Do Stage 8 only: Cairo platform integration and HiDPI.

Read ai/artist-cairo-backend-assimilation-plan.md Stage 8 section before starting.
Read ai/handoff.md.

Focus:
- Implement pre_scale using cairo_surface_set_device_scale.
- Target Linux as primary Cairo platform (X11/XCB or Wayland surface).
- Document macOS cairo-quartz as a secondary target.
- Validate HiDPI scaling on the primary platform.

Do not work on text shaping, image rewrites, or CI.

Update ai/handoff.md.
```
