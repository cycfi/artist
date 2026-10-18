# Release notes

High-level, user-facing changes only. For the day-to-day detail, see
`docs/dev_log.md`.

## v1.1.0 (in development, on `develop`)

Nothing yet.

## v1.0.0 (2026-10-18)

**The first versioned release.** Five backends (Quartz 2D, Direct2D, Skia,
Cairo and recording), each platform's native one by default, prebuilt Skia and
Cairo bundles, HarfBuzz text shaping on every backend, and the
[documentation](https://cycfi.github.io/artist/artist/v1.0/index.html) for the
whole API.

## Before 1.0

* 1 Jun 2026: All 15/15 Skia tests pass: fixed a Porter-Duff composite ops regression introduced by the m148 upgrade (`offscreen_image` now uses `SkSurfaces::Raster` instead of `SkPictureRecorder`; `drawPicture` with blend-mode paint is broken for Porter-Duff ops in m148). CI vcpkg builds now use the GitHub Actions binary cache (`x-gha`) for fast warm runs.
* 31 May 2026: Skia backend upgraded to m148 via vcpkg, replacing hand-managed prebuilt binaries; vcpkg added as a git submodule at `lib/external/vcpkg/`. Cairo Windows support wired up through the same vcpkg submodule. CI now runs the Cairo backend on Ubuntu, macOS, and Windows.
* 31 May 2026: Cairo backend gains HarfBuzz text shaping: OpenType ligatures, kerning, and correct complex-text advances for `fill_text`, `stroke_text`, `measure_text`, and `text_layout`.
* 30 May 2026: Cairo backend fully integrated: drop-shadow support, live window hosts on macOS, Linux, and Windows; visual test suite with CI on Ubuntu and Windows.
