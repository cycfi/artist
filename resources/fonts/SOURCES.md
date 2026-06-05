# Bundled test fonts

These fonts are bundled for the Artist test suite (visual regression and the
text-editor simulation tests). Both are under permissive licenses.

| Font | Files | License | Source |
|------|-------|---------|--------|
| Open Sans | `OpenSans-*.ttf`, `OpenSansCondensed-*.ttf` | Apache License 2.0 | https://github.com/googlefonts/opensans |
| Roboto Mono | `RobotoMono-Regular.ttf` | Apache License 2.0 | https://github.com/googlefonts/RobotoMono |

Roboto Mono (a monospace face) is used by the source-code editing test
(`test/text_edit_code_test.cpp`), which exercises text_layout_ex with soft-wrap
off — the scripts/configuration-file use case of elements #370.

Apache License 2.0: https://www.apache.org/licenses/LICENSE-2.0
