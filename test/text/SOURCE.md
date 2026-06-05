# Test text inputs

These files are inputs for the `text_layout` editing-simulation test
(`test/text_edit_sim_test.cpp`). They are chosen to cover the two profiles the
text editor must serve (cf. cycfi/elements#370): long soft-wrapping prose, and
source/config files with many hard line breaks.

## alice.txt

*Alice's Adventures in Wonderland* by Lewis Carroll (1865). **Public domain.**

Source: Project Gutenberg ebook #11 (https://www.gutenberg.org/ebooks/11). The
Project Gutenberg header/footer (trademark and license boilerplate) has been
removed; only the public-domain text of the work itself is retained. The text
was **reflowed**: each blank-line-delimited paragraph was joined into a single
logical line, so paragraphs are soft-wrapped by the layout engine via
`flow(width)` rather than carrying Gutenberg's fixed ~70-column hard wraps.
UTF-8, LF line endings. (~817 paragraphs, ~150 KB; smart quotes/dashes give
incidental non-ASCII shaping coverage.)

## code_sample.txt

A frozen snapshot of `lib/impl/cairo/canvas.cpp` from this repository.
**MIT-licensed (this project's own source.)** Represents the source-code /
configuration-file profile from the original #370 motivation: many hard newline
breaks, indentation, and mixed short/long lines (~1069 lines, ~37 KB). Kept
verbatim; it is a snapshot for deterministic golden testing and is intentionally
not kept in sync with the live source file.
