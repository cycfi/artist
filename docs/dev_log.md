# Artist development log

Newest first. Each entry records what landed on a branch, with the detail that
would otherwise have lived in commit bodies (commit messages are kept to a
single subject line). Hashes are the post-rewrite short hashes on the branch
named in the heading.

## 2026-06-14 -- master hotfixes

**TL;DR:** Small master-only fixes on top of the develop features README pass:
a portable Windows MSVC build and a docs-only CI skip.

- `fe3ef80` ci: fix Windows MSVC build -- use vswhere to locate Visual Studio
  instead of a hardcoded path; also widen .gitignore to cover `build-*/`,
  `vcpkg_installed/`, `docs/`, `ai/`.
- `38e9784` ci: skip build for docs-only changes (same as develop `391614f`).
- `d4a1a3e` README updated to describe the develop-branch features.

## 2026-06-13 -- CI: skip docs-only builds (develop `391614f`)

**TL;DR:** A push that touches only documentation no longer triggers a full
platform build matrix.

## 2026-06-07 -- native Linux hosts, resizable example hosts, and the CI examples/smoke matrix

**TL;DR:** Artist gains native Wayland and X11 hosts (no GTK dependency), all
example hosts across macOS/Linux/Windows become resizable and responsive, and CI
now builds the examples and smoke-tests every backend/platform/host combination.
The `artist_2026_skia_upgrade` branch was merged in to bring the Windows
resizable hosts onto develop.

Native Linux hosts (merged at `0d23f7a`):
- `77eef42` native Wayland host for Cairo (wl_shm double buffering) and Skia
  (EGL/GL via wl_egl_window + Ganesh). HiDPI at any fractional scale via
  `wp_fractional_scale_v1` + `wp_viewport`; 60fps frame-callback loop. libdecor
  is used for client-side decorations to avoid a libpng symbol clash with GDK's
  cairo plugin (static-lib symbol hiding applied for safety). Selectable with
  `ARTIST_LINUX_HOST=wayland`.
- `3d8f895` reorganize hosts under `host/linux/{gtk3,wayland,x11}/`; add a native
  X11 host (Cairo + Skia) on Xlib + EGL/GL, HiDPI read from `Xft.dpi`.
  `ARTIST_LINUX_HOST` selects the backend (default gtk3).
- `3a3d37e` all six Linux hosts (gtk3/wayland/x11 x cairo/skia) support live
  resize with coalesced surface recreation. `wp_viewport_set_destination`
  replaces the integer-only `wl_surface_set_buffer_scale` (which misrepresents
  fractional scales, causing overflow on Skia and a black screen on Cairo). Fixed
  a Wayland Cairo animation stall: the initial configure left the shm buffer
  busy, so a bare `wl_surface_commit` now arms the first frame callback instead of
  a no-op render.
- `b52248e` fix a GTK3+Skia pkg-config conflict: vcpkg injects harfbuzz 14.x into
  `PKG_CONFIG_PATH` while Ubuntu's pango.pc pins an exact system harfbuzz, so the
  gtk3 check fails; temporarily override `PKG_CONFIG_LIBDIR` to system-only paths
  around that check.
- `60edb44` remove dev instrumentation (`ARTIST_RESIZE_LOG/BENCH/DUMP_FRAME`);
  fix an X11 Cairo gray resize flash and a Wayland Skia startup flicker.

Resizable/responsive examples:
- `ab16dac` all macOS hosts (Quartz2D, Skia/Metal, Cairo) get resizable windows
  with live surface/layer recreation; add `scale_to_fit()` to app.hpp
  (letterboxed fixed-design scaling). Quartz2D `CGContextClipToRect` fixes
  `clip_extent()` reporting 32px too tall (AppKit included the title-bar strip).
- `7096f60` Windows hosts made resizable: skia_app (WGL) recreates the Skia
  surface on WM_SIZE; cairo_app repaints and sizes the offscreen buffer from
  `GetClientRect` (was `GetWindowRect`, overshooting by the title bar). Cairo had
  never built on Windows before this.
- `b5d2fdb` merge of `artist_2026_skia_upgrade` bringing the five Windows
  example/host files onto develop (skia/cairo hosts hardware-tested; composite_ops
  caches its static offscreen to fix Skia resize lag).

CI:
- `c0188f4` new build-linux-hosts job: all six host combos built with examples on,
  each binary run up to 3s under Xvfb (GTK3/X11) or headless weston (Wayland) to
  catch startup crashes. Would have caught both PR #27 issues (compile errors and
  the null `zxdg_decoration_manager_v1` segfault on GNOME).
- `484d670` enable examples build + smoke tests on every job (Skia, Cairo,
  Quartz2D) across platforms; a binary must exit 0 or be killed by timeout (124).
  Linux uses xvfb-run; Windows uses git-bash timeout.
- `ad63aac`, `6aef102`, `289f44e`, `59c0f31`, `2147144`, `2976f1d` iterative smoke
  fixes: macOS gtimeout (coreutils), xvfb-run subprocess function visibility,
  `set -e` firing on exit 124, `libfontconfig1-dev`, `composite_ops` global-image
  load ordering (moved to static locals so images load after `init_paths`),
  Mesa/llvmpipe software GL for headless Skia (Zink has no Vulkan driver), running
  from the binary's own directory so `resources/` resolves, vcpkg DLL PATH on
  Windows, and finally skipping the Windows Skia smoke (EGL/ANGLE needs a GPU).

## 2026-06-06 -- Antora docs site and text_layout header decoupling

**TL;DR:** A published Antora documentation site replaces the old Jekyll pages
(resolves #28, PR #31), and `text_layout.hpp` is made includable without a
graphics backend so the non-graphical engine tests build everywhere.

Docs:
- `965d765` bootstrap the Antora (AsciiDoc) site under `docs/`, mirroring q and
  elements (antora.yml, playbook, Cycfi supplemental-ui, Pages workflow). The
  setup guide is rewritten against vcpkg + CMake presets with the R2 Skia cache.
  Resolves #28 (CMake min is 3.21 not 3.7.2; VS 2022 required; Skia/Cairo/Quartz
  documented across the three platforms).
- `fbda2b5` merge PR #31. `7ae6a49` scope the playbook to develop (master has no
  `docs/` yet). `fa52d51` port backends/gallery/canvas/foundation pages from the
  old Jekyll site. `6d56ab1` point the README at the published Antora URLs.

text_layout header cleanup (arriving at the final clean form):
- `b8aff39` first made `draw` a member template so `text_layout.hpp` stayed
  backend-free, then `408a358` superseded it: the real blocker was that
  canvas/image/path only declared their `*_impl` under a backend macro, so the
  headers did not parse with no backend selected. Added an opaque `#else struct
  *_impl;` fallback so they are includable for declarations without a backend
  (impl held by pointer; drawing methods out-of-line). `draw` returns to a plain
  `draw(canvas&, point, color)`.
- `18df028` use `std::upper_bound` in `para_at_y` (matches `paragraph_index`).
- `541a8d9`, `c6d0ebd`, `91714a0`, `ecdecf7` declaration-hygiene: alias
  `break_enum` to keep names in the alignment column, move rationale out of the
  class body, align `operator=`, rename the template parameter `Layout` ->
  `TextRun` (the per-paragraph atomic shaped unit).

## 2026-06-05 -- rope-backed multi-paragraph text engine (elements #370) and text-layout correctness

**TL;DR:** A persistent rope buffer plus an incremental paragraph index makes
editing O(1) in document size instead of reshaping the whole document per
keystroke, and a cluster of cross-backend correctness fixes address ligatures,
trailing hard breaks, and CJK wrapping.

- `ef70b80` Rope-backed engine (squash of feature/text-engine-rope):
  - `rope<T>` (artist/rope.hpp): persistent, structurally-shared; O(log n)
    insert/erase/split, O(1) snapshot, leaf coalescing, chunked load/paste.
  - `paragraph_index`: incremental partition on hard `\n`.
  - `basic_text_layout` / `text_layout` (artist/text_layout.hpp): rope buffer +
    paragraph index + one shaped paragraph each; an edit reshapes only the touched
    paragraphs. `text_run` (artist/text_run.hpp) is the former single-paragraph
    text_layout. Also fixes a phantom blank line on wrap+hard-break and switches
    vertical accumulation to double across skia/cairo/quartz.
  - Tests (headless, per backend): rope/paragraph_index fuzz, real-backend layout
    equivalence + caret row hit-test, and two acid editing-session tests that
    reconstruct a novel-length prose doc and a source file edit-by-edit and assert
    byte-identical output. Green on skia/cairo/quartz x macOS/Linux/Windows.
- `4552432` open an empty line after a trailing hard break (#384): Skia/Quartz
  consumed the break but emitted no row for the empty line after it, so the caret
  could not land there (you had to press Return twice). Cairo already flushed a
  final row.
- `dab998f` don't keep a trailing newline glyph after a ligature (#384): the
  end-of-text-boundary guard also caught a hard break owning the last glyph, which
  must be consumed, not kept (it drew a .notdef box at the right edge). Restrict
  the guard to non-hard-break glyphs.
- `076087b` fix text disappearing when a string ends in a ligature (#384): Skia
  flushes the final line at the INDETERMINATE code point, but a trailing ligature
  puts the last glyph on an earlier code point so the marker was never visited and
  the whole line vanished. Treat the last buffer glyph as an end-of-text boundary
  too (mirrors Cairo).
- `3f10ff5` fix CJK dropped when wrapping (#430): the break code assumed the break
  opportunity was a consumable space and dropped the boundary glyph; for CJK each
  ideograph is its own break with nothing to consume, so the boundary glyph must
  stay. Skia also read one past the glyph array on the force-break path. Added a
  `consume` flag and a no-progress guard.

## 2026-06-04 -- cross-backend text/offset fixes

**TL;DR:** Two cross-backend consistency fixes: GTK+Cairo no longer clips the
top-left of every window, and `word_break` returns real Unicode word
segmentation on all backends.

- `71bac2c` GTK+Cairo content offset: Cairo used absolute
  device_to_user/user_to_device while Skia/Quartz compute them relative to the CTM
  captured at canvas construction. When a host hands the canvas a context whose
  CTM already encodes a base transform (GTK content-area offset), the whole view
  shifted and clipped. Capture the inverse initial CTM and compute relative to it;
  add a regression test.
- `4a16c0f` `word_break` returns the UAX-29 word table on all backends (Skia and
  Quartz2D had returned the line-break field), so double-click word selection
  handles contractions, decimals, grouping separators, and punctuation. Added a
  Word Selection test.

## 2026-06-02 -- Skia binary distribution, CI cache/matrix, harfbuzz interposition

**TL;DR:** Local builds default to a checksum-verified prebuilt Skia bundle from
R2 (no Skia compile), CI restructures around an R2 vcpkg binary cache and an
x86/arm matrix, and a GTK crash from static/dynamic harfbuzz interposition is
fixed by linking the system harfbuzz.

- `4811212` `cmake/SkiaPrebuilt.cmake`: on configure (no vcpkg toolchain), derive
  the os/arch triplet and download+verify+extract the prebuilt Skia bundle from
  `media.cycfi.com/skia-prebuilt/<ver>/<triplet>.tar.zst`, prepend it to
  `CMAKE_PREFIX_PATH`. Link-compatible across compiler versions (unlike vcpkg's
  ABI-exact cache); falls back to vcpkg.
- `674119e` gate the bundle on the local toolchain's compatibility class (Linux
  glibc >= 2.35, Windows MSVC v143, macOS deploy >= 11.0) with a clear "use the
  source preset" message; CMakePresets adds `prebuilt` (default) and `source`.
- `bc770b8`, `05b9b39`, `e497a8d`, `11a5053`, `9219d1f` CI cache/matrix: a shared
  cross-repo NuGet cache layered over x-gha, then an R2-only cache (http read +
  x-aws write) with an x64-windows/x64-linux/arm64-osx/arm64-linux Skia matrix
  mirroring elements; arm64-linux deferred pending an arm64 golden; Cairo's
  Windows vcpkg deps cached on R2.
- `f9db68d` Linux Skia: link the system harfbuzz. The `elseif(UNIX)` block linked
  vcpkg's static harfbuzz while GTK/Pango load the system one dynamically; symbol
  interposition routed Pango into the static 14.x copy and crashed on widget
  realize. Headless tests never load GTK so they passed, but Elements segfaulted
  on launch. Verified Elements now runs on Ubuntu 26.04 arm64.
- `238dea7` Cairo: use `cycfi::pi` instead of `M_PI` (undefined on MSVC without
  `_USE_MATH_DEFINES`), which broke the Windows Cairo build.
- `42f854b` setup-remote-dev doc: VMware Fusion procedure (real Wayland/GL vs
  OrbStack headless; arm64 harfbuzz requirement; Mesa-Zink notes) and the
  agent-over-SSH remote workflow. `392deff` move `ai/` notes to the private
  cycfi_ai_dev repo and gitignore local `ai/`.

## 2026-06-01 -- Skia m148 upgrade and Linux/Windows port

**TL;DR:** Skia is upgraded to milestone 148 via vcpkg and ported to Linux and
Windows, with a Win32 + WGL Ganesh example host and DirectWrite font matching.

- `6ff376d` upgrade Skia to m148 via vcpkg (macOS). `9460dfd` restructure CI into
  per-backend/platform jobs, wiring Cairo Windows via vcpkg. `9f2c3cb` switch
  vcpkg to the x-gha binary cache.
- `c696917` fix Skia composite ops by using a raster surface in
  `offscreen_image`. `6732725` update the Linux Skia tauri golden (m148 AA delta).
- `a23a262` port Skia to Linux via vcpkg. `06e6502` use system harfbuzz on Linux
  to avoid a static/dynamic conflict with GTK (precursor to `f9db68d`).
- `1c8f67d`, `7d5af13`, `109e50e` Windows Skia: C++20 build fixes, DirectWrite
  font manager, `SkPathBuilder`, DirectWrite font matching, goldens, and a
  Win32 + WGL Ganesh example host.
- `1972f1c` Cairo MSVC portability + Windows goldens. `d375f75`, `cd364e8`,
  `589d25f` port plan, cross-platform build/setup guide, and remaining-verification
  tracking.

## 2026-05-30 to 05-31 -- Cairo backend bring-up

**TL;DR:** A full Cairo backend lands across ten staged steps: paths, text and
fonts with HarfBuzz shaping, images, gradients, compositing, software Gaussian
shadows, HiDPI, and window hosts on all three platforms. macOS switches to a
Quartz cairo surface with CG-backed fonts, and the visual regression metric moves
from SSI to a tiled CIEDE2000 check.

Backend and stages:
- `b533032` initial Cairo build support; `a705857` visual smoke coverage
  (golden path, initial goldens, PNG dirty-surface crash, path clipping fill-rule,
  image clipping for unbounded operators).
- `89bd142` Stage 5 text/fonts: Fontconfig-resolved FreeType-backed Cairo scaled
  fonts, metrics/measure_text, word-wrapped `text_layout` with libunibreak.
- `ae91b80` Stages 6-7 images/pixels/gradients/compositing; fix lighter/darker
  ops to W3C semantics; extended blend-mode tests.
- `1aaeaa2` Stages 8-10 HiDPI + coordinate conversion, Cairo CI job, `make_image`
  pixel-buffer ctor, chessboard test moved into test/, Linux/Windows/macOS
  goldens.
- `714a28f` post-assimilation polish: Cairo path deep equality via
  `cairo_copy_path`, font_descr preserved across `text()` updates, hardened raw
  pixel access (flush before returning the pointer).
- `dacf6e9` Cairo window hosts for Linux/Windows/macOS (XCB/Win32 native
  surfaces; macOS CGBitmap blit).

Text shaping and shadows:
- `b52c215` HarfBuzz shaping: `shape_text` via `hb_buffer` + `hb_shape` with
  `hb_ot_font_set_funcs` (GSUB/kern/liga active); shaped advances feed
  measure_text so widths match; shaped glyph IDs/advances used in
  fill_text/stroke_text; `text_layout` reworked onto shaped runs (ligature
  clusters handled).
- `ddccf2d` `shadow_style` via a multi-pass box-blur Gaussian approximation
  (`approx_gaussian_blur_1ch`, SIMD inner loops), rendered at device resolution
  (scale-aware sigma); text/stroke shadows and glow; 4-pass blur fixes a clipped
  glow edge.
- `af63b49` cache shadow bitmaps in `canvas_state` to remove per-frame blur cost.

macOS Quartz optimization and font perf:
- `9a712af` port CG font face + `cairo_quartz_surface_create_for_cg_context` (no
  per-frame pixel-buffer copy); on macOS Quartz uses CG-backed faces (correct
  under isFlipped=YES), others use the FT scaled font. `308c439` a
  `cairo_move_to` before `cairo_show_glyphs` is required on Quartz or glyphs
  silently do not render.
- `2f20258` restore `HINT_METRICS_OFF` in non-macOS scaled-font creation (the
  Quartz pass had introduced a recording-surface path that inherited hint-metrics
  ON, rounding ascent/descent up one pixel on Linux).
- `9b13388` replace `FcFontMatch` (a full Fontconfig search on every
  `font(font_descr)`, dominating CPU) with a pre-enumerated font map + face cache
  keyed by `FC_FULLNAME`, matching the Skia/Elements strategy.
- `1c66f30` decouple the HarfBuzz face from the FreeType lifecycle: build the
  `hb_face_t` from a plain data blob (`hb_blob_create_from_file` +
  `hb_face_create`) so `hb_face_destroy` at exit frees only memory and never calls
  `FT_Done_Face` after the FreeType dylib has started teardown (was a SIGSEGV).

Quartz2D fixes and the test metric:
- `8a23d6a` Quartz2D `path::operator==` now compares `_fill_rule`
  (`CGPathEqualToPath` is geometry-only); `make_image` `pixels()` uses
  `initWithCGImage:` so CGImage-backed images expose bitmap data.
- `5ae2e62` replace the SSI visual-regression metric with CIEDE2000 (dE00,
  sRGB->LAB) plus a complexity-adaptive 32x32 tiled check (per-tile tolerance from
  local luminance variance).

## Direct2D backend (branch `direct2d-2026`, not yet merged to develop)

**TL;DR:** A native Windows Direct2D + DirectWrite + WIC backend, the Windows twin
of Quartz2D, dependency-free (system libs only, no Skia blob, no vcpkg). Complete
and CI-green on origin; rebased onto the rewritten develop and awaiting a PR.
(These commits carry today's committer date as an artifact of the rebase; the work
dates to 2026-06-07/08.)

- `ee6fbe5` port the reusable 2020 Direct2D core (context, geometry-generator
  path_impl, canvas paths/gradients/strokes/shadow) onto develop's `canvas_impl`
  seam; `ARTIST_DIRECT2D` CMake option links d2d1/dwrite/windowscodecs/dxgi/
  dxguid/ole32. Fills gaps vs 2020: real save/restore stack, lazy brushes rebuilt
  on device loss, full transforms, clear_rect, clip_extent.
- `934073e` WIC image (create/load/from-memory, save_png, premultiplied-BGRA
  pixels matching the golden comparator) and `offscreen_image`
  (`CreateWicBitmapRenderTarget`) so the canvas renders fully headless. Fixes the
  2020 branch's broken radial gradient mapping.
- `e43496c`, `736092e` DirectWrite font (font_impl, metrics, measure_text, dwrite
  factory) and canvas text (fill/stroke/measure, alignment).
- `6aaf58d` DirectWrite `text_run` engine: per-paragraph `IDWriteTextLayout`
  backing the shared engine; libunibreak breaks; u32<->u16 caret mapping;
  `current_matrix()` so text honors the transform; `/EHsc` enabled (a throw was
  fail-fasting `0xC0000409`).
- `3ce8fc9` caret_index honors the engine's top-based row contract (DirectWrite
  `HitTestPoint` is body-based, landing one row high); `3bdbb22` load bundled
  resource fonts via a custom `IDWriteFontCollection` (DirectWrite exposes only
  system fonts); `cbfba50` make `save_png` null-safe and create the parent dir
  (found via ASan; the full-novel editing test then passes at 9780 assertions).
- `4ea9657` `add_path` + a safe `path_impl` copy ctor (skip realized-geometry COM
  caches to avoid a double free); `523888f` clip()/clip(path) via PushLayer with a
  save/restore-nested layer stack.
- `40bd21a` working drop shadows: create the factory as `ID2D1Factory1` so the
  target can be Queried to `ID2D1DeviceContext` for effects; `apply_blur` now
  composites onto the main target. `850ff30` match Skia's blur (sigma = blur, not
  blur/2). `8885b3f` fix per-corner round-rect (no spurious edge from the origin;
  collapse a non-positive radius to a point).
- `dcce9d7` require MSVC v14.44+: the "shapes2 / Drawing2" spurious geometry in
  optimized builds was an MSVC v14.39 optimizer codegen bug, not UB (v14.44 and
  clang-cl 19 render byte-identically; Debug/ASan always clean). Gate with a
  FATAL_ERROR and re-enable IPO/LTO.
- `35970f5` all 24 `global_composite_operation` ops: the 11 Porter-Duff via
  `DrawImage` + `D2D1_COMPOSITE_MODE`, the 13 blend via `CLSID_D2D1Blend` over a
  `CopyFromRenderTarget` snapshot, confined to the primitive's device bounds. Also
  fix clip() to consume the current path (latent bug exposed by composite_test).
- `2340822` `path::operator==` via a structural op log; `0f05557` caret_index row
  mapping from `HitTestTextPosition`, npos below the last line, left-edge snap for
  zero-width line starts.
- `58ec5f5` seed `windows_golden/direct2d` (each render verified vs Skia);
  `artist_test` passes 27680 assertions across 29 cases. `401f011` add the
  Direct2D (Windows MSVC) CI job (system libs, headless via WARP, v14.44 gate).
- `a88f5b0`, `20e7335` per-monitor-v2 DPI + WM_DPICHANGED for the Direct2D host and
  the Cairo/Skia Windows hosts; `3ae6ca5` condensed-font family resolution and text
  drop-shadow/glow (re-seeded the typography golden).
