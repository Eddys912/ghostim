# Changelog

All notable changes to ghostim are documented here.
Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
Versioning follows [Semantic Versioning](https://semver.org/).

---

## [1.0.0] — 2026

### Added

- `info` command: display all metadata found in JPEG and PNG files.
- `clean` command: remove all non-image data from JPEG and PNG files.
- `--strip gps`: remove only GPS coordinates, preserve other EXIF.
- `--output <dir>`: write cleaned files to a specified directory.
- `--dry-run`: preview what would be removed without writing.
- `--verbose`: show every segment or chunk removed with its size.
- JPEG support: removes APP0, APP1 (EXIF/XMP), APP3–APPF, COM segments.
- PNG support: removes eXIf, tEXt, iTXt, zTXt, tIME chunks.
- ICC color profile (APP2) always preserved.
- Atomic write via temp file + rename — no partial output on failure.
- Cross-platform: Linux, macOS (universal), Windows.
- GitHub Actions workflow for automated binary releases on version tags.
