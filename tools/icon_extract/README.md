# icon-extract

icon-extract extracts classic Macintosh icons and writes them as PNG files.

It is designed for icon data that comes from:
- Standalone .icns files
- AppleDouble sidecars (for example files named ._Name)
- Raw Macintosh resource-fork blobs

For each input, it picks the best icon representation available and writes a PNG.
When possible, it prefers modern icon data and falls back to classic icon resources.

## What It Does

At a high level, icon-extract:
- Reads input files (or scans directories with -r)
- Finds icon data in .icns or resource forks
- Extracts the largest usable icon variant
- Writes PNG files to the output directory
- Adds simple metadata (Title and Source) to written PNGs

## Usage

Usage:
  icon-extract [OPTIONS] FILE|DIR [FILE|DIR...]

Options:
  -o, --output-dir DIR    Write PNGs to DIR (default: current directory)
  -r, --recursive         Recurse into directories
  -v, --verbose           Print each extracted icon
  -h, --help              Show help

## Notes About Directory Scanning

With -r, directory scanning currently includes files that are:
- AppleDouble sidecars named with the ._ prefix
- .icns files

If your extraction tool writes sidecars as .rsrc files instead of ._ files,
pass those .rsrc files directly as inputs, or use an unpacking mode that keeps
AppleDouble as ._ files.

## Example: ZIP From macOS With Folder Icons

Many ZIP files created on macOS store icon/resource metadata in AppleDouble files
under __MACOSX/.

1) Unpack the archive:
   unzip /tmp/Icones.zip -d /tmp/sample_unzip

2) Run icon-extract on the unzip root (not only the data folder):
   bld/linux/icon-extract -r -o /tmp/out_icons /tmp/sample_unzip

Why the unzip root matters:
- AppleDouble sidecars are often under __MACOSX/
- Running on only /tmp/sample_unzip/Icones can miss those sidecars

## Output

- Output files are PNG images.
- If a filename already exists, icon-extract appends " (2)", " (3)", and so on.
- The tool exits with a non-zero status if no icons are extracted.
