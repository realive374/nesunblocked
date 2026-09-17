# Smile and Snarl — *Crittrrific Switcheroo*, Book Five: *The Eight O'Clock Hour*

Source and build script for the Word document in `../`.

## Contents

- `ms_01.txt` … `ms_05.txt` — the manuscript, in reading order. Plain text with three markers:
  - `@PART|PART ONE|THE WORD` — a part title page
  - `@CH|1|A Perfectly Ordinary Morning in Playcare` — a chapter opening
  - any other non-blank line — one paragraph of body text
- `build.py` — builds the `.docx` with python-docx, renders it to PDF, and reports the page count.

## Rebuilding

```bash
pip install python-docx
python3 build.py '{"size":13,"lead":1.771,"bottom":0.72,"pspace":0.34}' --with-toc
```

Those are the exact parameters that produce **264 pages**. The script writes `book5.docx`
next to itself and prints the measured page count.

## How the page count is verified

`build.py` converts the `.docx` to PDF with LibreOffice and reads the page count with
`pdfinfo`, so the number is measured from a real render rather than estimated.

Requires `libreoffice-writer` and `poppler-utils`.

## Two things worth knowing before editing

**The font is Times New Roman on purpose.** The page count only holds if the renderer has
the same metrics the document was tuned against. Times New Roman is metrically identical to
Liberation Serif, which is what Linux/LibreOffice substitutes, so the count measured on a
build machine matches what Word shows. Swapping in a font with different metrics (Georgia,
Garamond) will change the page count.

**The table of contents is generated in two passes.** The first pass builds the book without
page numbers and records where each `CHAPTER n` heading actually landed in the PDF
(`starts.json`). Copy that to `toc.json` and rebuild with `--with-toc` to typeset the real
numbers. The script re-measures afterwards to confirm the numbers didn't shift the pagination.

## Trim

6" × 9", 0.7" top / 0.72" bottom / 0.72" sides, 13pt on 1.771 line spacing, ragged right.
No images.
