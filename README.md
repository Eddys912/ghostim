<div align="center">
  <h1>ghostim</h1>
  <p>Image metadata remover for <strong>JPEG</strong> and <strong>PNG</strong> files</p>

![Arch](https://img.shields.io/badge/Arch-1793D1?logo=archlinux&logoColor=1793D1&labelColor=fff&color=1793D1)
![C](https://img.shields.io/badge/C-00599C?logo=c&logoColor=00599C&labelColor=fff&color=00599C)

</div>

## Description

ghostim removes all metadata embedded in JPEG and PNG files — EXIF data, GPS coordinates, camera model, timestamps, comments, and vendor segments — without modifying a single pixel of the image.

It works by parsing the raw binary format directly, with **zero external dependencies**. Only the C standard library is used.

What gets removed:

- **EXIF** — camera make/model, date, software, orientation
- **GPS** — latitude and longitude coordinates
- **Thumbnails** — embedded low-resolution preview copies
- **Comments** — text segments added by editors or apps
- **Vendor segments** — proprietary APP data from cameras and software

What is always preserved:

- **Pixel data** — the actual image, untouched
- **ICC color profile** — preserves color accuracy

## Requirements

- **OS**: Linux, macOS, or Windows
- **C Compiler**: GCC or Clang (C99)
- **Build**: CMake ≥ 3.15
- **Optional**: Just (command runner for shortcuts)

## Installation

### Arch Linux / WSL

```bash
sudo pacman -S cmake gcc just
```

### Debian / Ubuntu

```bash
sudo apt install cmake gcc
# just: https://just.systems
```

## Build

```bash
git clone https://github.com/YOUR_USERNAME/ghostim.git
cd ghostim

# With Just (recommended)
just build      # debug + sanitizers
just release    # optimized binary

# With CMake directly
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Binary output: `build/ghostim`

## Usage

```bash
ghostim <command> [options] <file(s)>
```

### Commands

| Command | Description |
| `info <file>` | Show all metadata found in a file |
| `clean <file(s)>` | Remove all metadata from file(s) |

### Options

| Option | Description |
| `--strip gps` | Remove only GPS coordinates |
| `--output <dir>` | Write cleaned files to a directory |
| `--dry-run` | Preview what would be removed, without writing |
| `--verbose` | Show every segment removed |

### Examples

```bash
# Inspect a photo before cleaning
ghostim info photo.jpg

# Remove all metadata (overwrites in place)
ghostim clean photo.jpg

# Remove only GPS, keep other EXIF
ghostim clean photo.jpg --strip gps

# Clean a batch into a separate folder
ghostim clean *.jpg --output ./clean/

# Preview without writing
ghostim clean photo.jpg --dry-run --verbose
```

### With Just

```bash
just info photo.jpg     # show metadata
just clean photo.jpg    # remove all metadata
just dry photo.jpg      # preview without writing
```

## Download

Pre-built binaries are available on the [Releases](../../releases/latest) page.

| Platform | File |
| Linux x86-64 | `ghostim-linux-x86_64` |
| macOS (Intel + Apple Silicon) | `ghostim-macos-universal` |
| Windows x86-64 | `ghostim-windows-x86_64.exe` |

```bash
# Linux / macOS: make executable first
chmod +x ghostim-linux-x86_64
```

## How It Works

### JPEG

JPEG files are a sequence of markers (`FF XX`). Each metadata segment is a separate marker before the compressed scan data. ghostim walks every marker, identifies and removes non-image segments (APP0, APP1, APP3–APPF, COM), and writes the rest verbatim. The compressed pixel data after `SOS` is never read or modified.

### PNG

PNG files are a sequence of chunks (`length + type + data + CRC`). ghostim reads each chunk header, drops metadata chunks (`eXIf`, `tEXt`, `iTXt`, `zTXt`, `tIME`), and copies the rest unchanged. `IDAT` (pixel data) is never touched.

<div align="center">
  <br>
  <img
    src="https://img.shields.io/badge/Zero%20dependencies-Pure%20C99-00599C?style=for-the-badge"
    alt="Zero dependencies — Pure C99"
  />
  <br><br>
  <p>⭐ <strong>Star this repository if it was useful</strong> ⭐</p>
</div>
