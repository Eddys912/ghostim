# install.ps1 — Ghostimg installer for Windows
# Usage: irm https://raw.githubusercontent.com/USER/ghostimg/main/install.ps1 | iex

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# ── Config ─────────────────────────────────────────────────────────────────
$Repo      = "edavnix/ghostimg"
$Binary    = "ghostimg"
$Version   = "1.0.0"
$InstallDir = "$env:USERPROFILE\AppData\Local\Programs\ghostimg"

# ── Helpers ─────────────────────────────────────────────────────────────────
function Write-Info    { param($msg) Write-Host "info   $msg" -ForegroundColor Yellow }
function Write-Success { param($msg) Write-Host "ok     $msg" -ForegroundColor Green  }
function Write-Fail    { param($msg) Write-Host "error  $msg" -ForegroundColor Red; exit 1 }

# ── Download binary ─────────────────────────────────────────────────────────
function Get-Binary {
    $filename = "${Binary}-windows-x86_64.exe"
    $url      = "https://github.com/${Repo}/releases/download/v${Version}/${filename}"
    $tmp      = Join-Path $env:TEMP "${Binary}.exe"

    Write-Info "Downloading ${filename} from GitHub Releases..."
    try {
        $ProgressPreference = "SilentlyContinue"
        Invoke-WebRequest -Uri $url -OutFile $tmp -UseBasicParsing
    } catch {
        Write-Fail "Download failed. Check your connection or visit:`n  https://github.com/${Repo}/releases"
    }

    return $tmp
}

# ── Install binary ──────────────────────────────────────────────────────────
function Install-Binary {
    param($tmp)

    if (-not (Test-Path $InstallDir)) {
        New-Item -ItemType Directory -Path $InstallDir -Force | Out-Null
    }

    $dest = Join-Path $InstallDir "${Binary}.exe"
    Move-Item -Path $tmp -Destination $dest -Force

    Write-Success "Installed to $dest"
    return $dest
}

# ── Ensure install dir is in user PATH ──────────────────────────────────────
function Add-ToPath {
    $currentPath = [Environment]::GetEnvironmentVariable("PATH", "User")

    if ($currentPath -like "*$InstallDir*") {
        return
    }

    Write-Info "Adding $InstallDir to user PATH..."
    $newPath = "$currentPath;$InstallDir"
    [Environment]::SetEnvironmentVariable("PATH", $newPath, "User")

    # Also update current session
    $env:PATH = "$env:PATH;$InstallDir"

    Write-Success "PATH updated — no restart required"
}

# ── Verify installation ──────────────────────────────────────────────────────
function Test-Install {
    param($dest)

    try {
        $ver = & $dest --version 2>&1
        Write-Success "Installation verified — $ver"
    } catch {
        Write-Fail "Installation failed — binary not executable."
    }
}

# ── Main ────────────────────────────────────────────────────────────────────
function Main {
    Write-Host ""
    Write-Host "ghostimg installer  v${Version}" -ForegroundColor White
    Write-Host ""

    $tmp  = Get-Binary
    $dest = Install-Binary $tmp
    Add-ToPath
    Test-Install $dest

    Write-Host ""
    Write-Host "Done. " -ForegroundColor White -NoNewline
    Write-Host "Run " -NoNewline
    Write-Host "ghostimg --help" -ForegroundColor Cyan -NoNewline
    Write-Host " to get started."
    Write-Host ""
    Write-Host "Note: if ghostimg is not found, open a new terminal window." -ForegroundColor Yellow
    Write-Host ""
}

Main
