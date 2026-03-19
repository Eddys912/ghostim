#!/usr/bin/env bash
# install.sh — Ghostimg installer for Linux and macOS
# Usage: curl -fsSL https://raw.githubusercontent.com/USER/ghostimg/main/install.sh | bash

set -euo pipefail

# ── Config ─────────────────────────────────────────────────────────────────
REPO="edavnix/ghostimg"
BINARY="ghostimg"
INSTALL_DIR="${HOME}/.local/bin"
VERSION="1.0.0"

# ── Colors ─────────────────────────────────────────────────────────────────
if [ -t 1 ]; then
    RED='\033[0;31m'; GREEN='\033[0;32m'
    YELLOW='\033[0;33m'; CYAN='\033[0;36m'
    BOLD='\033[1m'; RESET='\033[0m'
else
    RED=''; GREEN=''; YELLOW=''; CYAN=''; BOLD=''; RESET=''
fi

info()    { echo -e "${YELLOW}info   ${RESET}${1}"; }
success() { echo -e "${GREEN}ok     ${RESET}${1}"; }
error()   { echo -e "${RED}error  ${RESET}${1}" >&2; exit 1; }

# ── Detect OS and architecture ──────────────────────────────────────────────
detect_target() {
    local os arch

    case "$(uname -s)" in
        Linux)  os="linux"  ;;
        Darwin) os="macos"  ;;
        *)      error "Unsupported OS: $(uname -s)" ;;
    esac

    case "$(uname -m)" in
        x86_64)          arch="x86_64"   ;;
        arm64|aarch64)   arch="arm64"    ;;
        *)               error "Unsupported architecture: $(uname -m)" ;;
    esac

    if [ "$os" = "macos" ]; then
        echo "macos-universal"
    else
        echo "${os}-${arch}"
    fi
}

# ── Check dependencies ──────────────────────────────────────────────────────
check_deps() {
    if ! command -v curl &>/dev/null; then
        error "curl is required but not installed."
    fi
}

# ── Download binary ─────────────────────────────────────────────────────────
download() {
    local target="$1"
    local url="https://github.com/${REPO}/releases/download/v${VERSION}/${BINARY}-${target}"
    local tmp
    tmp="$(mktemp)"

    info "Downloading ${BINARY}-${target} from GitHub Releases..."
    if ! curl -fsSL --progress-bar "$url" -o "$tmp"; then
        rm -f "$tmp"
        error "Download failed. Check your connection or visit:\n  https://github.com/${REPO}/releases"
    fi

    echo "$tmp"
}

# ── Install binary ──────────────────────────────────────────────────────────
install_binary() {
    local tmp="$1"

    mkdir -p "$INSTALL_DIR"
    chmod +x "$tmp"
    mv "$tmp" "${INSTALL_DIR}/${BINARY}"

    success "Installed to ${INSTALL_DIR}/${BINARY}"
}

# ── Ensure install dir is in PATH ───────────────────────────────────────────
ensure_path() {
    # Check if already in PATH
    if echo "$PATH" | grep -q "$INSTALL_DIR"; then
        return
    fi

    info "Adding ${INSTALL_DIR} to PATH..."

    local shell_rc=""
    case "${SHELL:-}" in
        */zsh)  shell_rc="${HOME}/.zshrc"    ;;
        */fish) shell_rc="${HOME}/.config/fish/config.fish" ;;
        *)      shell_rc="${HOME}/.bashrc"   ;;
    esac

    if [ -n "$shell_rc" ]; then
        echo "" >> "$shell_rc"
        echo "# ghostimg" >> "$shell_rc"
        echo 'export PATH="$HOME/.local/bin:$PATH"' >> "$shell_rc"
        echo ""
        echo -e "  ${YELLOW}Reload your shell or run:${RESET}"
        echo -e "  ${CYAN}source ${shell_rc}${RESET}"
        echo ""
    fi
}

# ── Verify installation ──────────────────────────────────────────────────────
verify() {
    if "${INSTALL_DIR}/${BINARY}" --version &>/dev/null; then
        local ver
        ver=$("${INSTALL_DIR}/${BINARY}" --version)
        success "Installation verified — ${ver}"
    else
        error "Installation failed — binary not executable."
    fi
}

# ── Main ────────────────────────────────────────────────────────────────────
main() {
    echo ""
    echo -e "${BOLD}ghostimg installer${RESET}  v${VERSION}"
    echo ""

    check_deps

    local target
    target="$(detect_target)"
    info "Detected target: ${target}"

    local tmp
    tmp="$(download "$target")"

    install_binary "$tmp"
    ensure_path
    verify

    echo ""
    echo -e "${BOLD}Done.${RESET} Run ${CYAN}ghostimg --help${RESET} to get started."
    echo ""
}

main
