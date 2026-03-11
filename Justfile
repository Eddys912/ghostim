# Justfile for ghostim
# Documentation: https://just.systems/man/en/

set shell := ["bash", "-euo", "pipefail", "-c"]

# Build config
BUILD_DIR := "build"
BINARY    := BUILD_DIR + "/ghostim"

# Colors
RED    := '\033[0;31m'
GREEN  := '\033[0;32m'
YELLOW := '\033[0;33m'
CYAN   := '\033[0;36m'
GRAY   := '\033[0;90m'
END    := '\033[0m'

# Status prefixes
ERROR   := RED    + "ERROR  " + END
INFO    := YELLOW + "INFO   " + END
SUCCESS := GREEN  + "SUCCESS" + END
EXEC    := CYAN   + "EXEC   " + END

default: help

# Build ghostim (debug mode with sanitizers)
[no-exit-message]
build:
  #!/usr/bin/env bash
  echo -e "{{INFO}} Building ghostim (debug)..."
  cmake -B {{BUILD_DIR}} -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
  cmake --build {{BUILD_DIR}}
  echo -e "{{SUCCESS}} Binary ready: {{BINARY}}"

# Build ghostim (release — optimized, stripped)
[no-exit-message]
release:
  #!/usr/bin/env bash
  echo -e "{{INFO}} Building ghostim (release)..."
  cmake -B {{BUILD_DIR}} -DCMAKE_BUILD_TYPE=Release
  cmake --build {{BUILD_DIR}}
  echo -e "{{SUCCESS}} Binary ready: {{BINARY}}"

# Show metadata for an image. Usage: just info <file>
[no-exit-message]
info file:
  #!/usr/bin/env bash
  [[ ! -f "{{BINARY}}" ]] && {
    echo -e "{{ERROR}} Not built yet. Run: just build"
    exit 1
  }
  {{BINARY}} info "{{file}}"

# Clean all metadata from an image. Usage: just clean <file>
[no-exit-message]
clean file:
  #!/usr/bin/env bash
  [[ ! -f "{{BINARY}}" ]] && {
    echo -e "{{ERROR}} Not built yet. Run: just build"
    exit 1
  }
  {{BINARY}} clean "{{file}}"

# Preview what clean would remove without writing. Usage: just dry <file>
[no-exit-message]
dry file:
  #!/usr/bin/env bash
  [[ ! -f "{{BINARY}}" ]] && {
    echo -e "{{ERROR}} Not built yet. Run: just build"
    exit 1
  }
  {{BINARY}} clean "{{file}}" --dry-run --verbose

# Remove build directory
[no-exit-message]
wipe:
  #!/usr/bin/env bash
  rm -rf {{BUILD_DIR}}
  echo -e "{{SUCCESS}} Build directory removed"

# Show help
help:
  @echo -e "{{INFO}} Usage:"
  @echo -e "  just build         {{CYAN}}# Build (debug + sanitizers)    {{END}}"
  @echo -e "  just release       {{CYAN}}# Build (release, optimized)    {{END}}"
  @echo -e "  just info <file>   {{CYAN}}# Show metadata in an image     {{END}}"
  @echo -e "  just clean <file>  {{CYAN}}# Remove all metadata           {{END}}"
  @echo -e "  just dry <file>    {{CYAN}}# Preview clean without writing {{END}}"
  @echo -e "  just wipe          {{CYAN}}# Delete build directory        {{END}}"
