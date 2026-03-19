![Ghostimg](assets/banner.png)

<div align="center">
  <h1>Ghostimg</h1>
  <p>Cross-platform CLI tool to <strong>strip metadata</strong> and <strong>optimize images</strong> - JPEG, PNG, and WebP</p>

![Linux](https://img.shields.io/badge/Linux-FCC624?logo=linux&logoColor=000&labelColor=fff&color=FCC624)
![macOS](https://img.shields.io/badge/macOS-000000?logo=apple&logoColor=000&labelColor=fff&color=000000)
![Windows](https://img.shields.io/badge/Windows-0078D4?logo=windows&logoColor=0078D4&labelColor=fff&color=0078D4)
![C](https://img.shields.io/badge/C99-00599C?logo=c&logoColor=00599C&labelColor=fff&color=00599C)

</div>

## Description

Ghostimg removes all metadata embedded in JPEG and PNG files — EXIF data, GPS coordinates, camera model, timestamps, comments, and vendor segments — without modifying a single pixel of the image.

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

## Installation Requirements

- **Operating System**: Linux, macOS, or Windows
- **Architecture**: x86-64 (64-bit).
- **C Compiler GCC**: version 15.2.1 or higher.
- **C Debugger GDB**: version 17.1 or higher.
- **C Build CMake**: version 4.2.3 or higher.
- **Just command runner**: version 1.46.0 or higher.

## Usage

```bash
ghostimg <command> [options] <file(s)>
```

### Commands

| Command           | Description                       |
| ----------------- | --------------------------------- |
| `info <file>`     | Show all metadata found in a file |
| `clean <file(s)>` | Remove all metadata from file(s)  |

### Options

| Option           | Description                                    |
| ---------------- | ---------------------------------------------- |
| `--strip gps`    | Remove only GPS coordinates                    |
| `--output <dir>` | Write cleaned files to a directory             |
| `--dry-run`      | Preview what would be removed, without writing |
| `--verbose`      | Show every segment removed                     |

### Examples

```bash
# Inspect a photo before cleaning
ghostimg info photo.jpg

# Remove all metadata (overwrites in place)
ghostimg clean photo.jpg

# Remove only GPS, keep other EXIF
ghostimg clean photo.jpg --strip gps

# Clean a batch into a separate folder
ghostimg clean *.jpg --output ./clean/

# Preview without writing
ghostimg clean photo.jpg --dry-run --verbose
```

## Download

Pre-built binaries are available on the [Releases](../../releases/latest) page.

| Platform                      | File                          |
| ----------------------------- | ----------------------------- |
| Linux x86-64                  | `ghostimg-linux-x86_64`       |
| macOS (Intel + Apple Silicon) | `ghostimg-macos-universal`    |
| Windows x86-64                | `ghostimg-windows-x86_64.exe` |

```bash
# Linux / macOS: make executable first
chmod +x ghostimg-linux-x86_64
```

## How It Works

### JPEG

JPEG files are a sequence of markers (`FF XX`). Each metadata segment is a separate marker before the compressed scan data. Ghostimg walks every marker, identifies and removes non-image segments (APP0, APP1, APP3–APPF, COM), and writes the rest verbatim. The compressed pixel data after `SOS` is never read or modified.

### PNG

PNG files are a sequence of chunks (`length + type + data + CRC`). Ghostimg reads each chunk header, drops metadata chunks (`eXIf`, `tEXt`, `iTXt`, `zTXt`, `tIME`), and copies the rest unchanged. `IDAT` (pixel data) is never touched.

## Execution Instructions

### Arch Linux/WSL (Recommended)

1. **Clone the repository**:
   ```bash
   git clone https://github.com/edavnix/ghostimg.git
   ```
   > **Note**: When using Arch Linux natively, proceed to **Step 4**. For Windows environments, complete all steps to install and configure WSL Arch.
2. **Download and install WSL Arch** (PowerShell):
   ```bash
   wsl --install -d archlinux
   ```
3. **Restart the system** and access Arch Linux.
4. **Install GDB, compilation tools and Just**:
   ```bash
   pacman -Syu
   pacman -S gdb base-devel cmake just
   ```
5. **Verify installation**:
   ```bash
   gcc --version      # e.g. gcc (GCC) 15.2.1
   gdb --version      # e.g. GNU gdb (GDB) 17.1
   cmake --version    # e.g. cmake version 4.2.3
   just --version     # e.g. just 1.46.0
   uname -m           # e.g. x86_64
   ```
6. **Navigate to the directory**:
   ```bash
   cd ghostimg
   ```
7. **Execute exercises** use `just run` followed by the file name or path:
   ```bash
   just                   # view available commands
   just build             # debug and sanitizers
   just release           # optimized binary
   just info photo.png    # display metadata
   just clean photo.png   # remove all metadata
   just dry photo.png     # preview clean without writing
   just wipe              # delete build directory
   ```

<div align="center">
  <br>
  <img
    src="https://img.shields.io/badge/Made%20with-C%20%26%20Systems-00599C?style=for-the-badge"
    alt="Made with C"
  />
  <br><br>
  <p>⭐ <strong>Star this repository to show support</strong> ⭐</p>
</div>
