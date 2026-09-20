#!/bin/sh
# Vlicx Editor — Clean Installer & Updater
# Usage (One-liner from GitHub):
#   curl -fsSL https://raw.githubusercontent.com/cortane/Vlicx-Editor/main/install.sh | sh
# Or locally:
#   ./install.sh

set -e

PREFIX="${1:-/usr/local}"
REPO_URL="https://github.com/cortane/Vlicx-Editor"
RAW_REPO="https://raw.githubusercontent.com/cortane/Vlicx-Editor/main"
TAR_URL="https://github.com/cortane/Vlicx-Editor/archive/refs/heads/main.tar.gz"

echo "\033[1;36m=======================================================\033[0m"
echo "\033[1;36m   Vlicx Editor — Auto Clean Installer & Updater      \033[0m"
echo "\033[1;36m=======================================================\033[0m"

# Step 1: Cleanup old binary files and PATH entries
echo ""
echo "\033[1;33m[1/5] Cleaning up old binaries and temporary files...\033[0m"
rm -f /usr/local/bin/vlix* /usr/local/bin/vlicx* /usr/bin/vlix* /usr/bin/vlicx* 2>/dev/null || true
hash -r 2>/dev/null || true

# Step 2: Ensure build dependencies (Alpine Linux)
echo ""
echo "\033[1;33m[2/5] Checking build dependencies...\033[0m"
if command -v apk >/dev/null 2>&1; then
    MISSING_PKGS=""
    for pkg in gcc musl-dev ncurses-dev make curl tar; do
        if ! apk info -e $pkg >/dev/null 2>&1; then
            MISSING_PKGS="$MISSING_PKGS $pkg"
        fi
    done
    if [ -n "$MISSING_PKGS" ]; then
        echo "Installing missing packages:$MISSING_PKGS"
        apk add --no-cache $MISSING_PKGS
    else
        echo "All required Alpine packages are installed."
    fi
fi

# Step 3: Source acquisition
echo ""
echo "\033[1;33m[3/5] Acquiring source code...\033[0m"
TMP_DIR=""
HERE=""

if [ -f "./src/main.c" ] && [ -f "./Makefile" ]; then
    HERE="$(pwd)"
    echo "Using local source tree at: $HERE"
else
    TMP_DIR="/tmp/vlicx-build-$$"
    mkdir -p "$TMP_DIR"
    echo "Downloading latest Vlicx source archive from GitHub..."
    curl -fsSL "$TAR_URL" | tar -xzv -C "$TMP_DIR"
    MAKEFILE_LOC=$(find "$TMP_DIR" -path "*/vlicx/Makefile" 2>/dev/null | head -n 1)
    if [ -z "$MAKEFILE_LOC" ]; then
        MAKEFILE_LOC=$(find "$TMP_DIR" -name "Makefile" 2>/dev/null | head -n 1)
    fi
    if [ -n "$MAKEFILE_LOC" ]; then
        HERE=$(dirname "$MAKEFILE_LOC")
    fi
    if [ -z "$HERE" ] || [ ! -f "$HERE/Makefile" ]; then
        echo "Error: Could not locate Makefile in downloaded archive."
        exit 1
    fi
    echo "Located source tree at: $HERE"
fi

# Step 4: Build and Install
echo ""
echo "\033[1;33m[4/5] Building and installing Vlicx...\033[0m"
cd "$HERE"
make clean
make
make install PREFIX="$PREFIX"

# Install Japanese SKK dictionary
JISYO_DIR="$HOME/.vlicx-jisyo"
JISYO_SRC="$HERE/src/prtxt"
JISYO_DST="$JISYO_DIR/prtxt"

mkdir -p "$JISYO_DIR"
if [ -f "$JISYO_SRC" ]; then
    cp "$JISYO_SRC" "$JISYO_DST"
    echo "Installed Japanese dictionary to: $JISYO_DST"
fi

# Step 5: Save version SHA for auto-update checks
echo ""
echo "\033[1;33m[5/5] Recording installed version info...\033[0m"
VERSION_FILE="$HOME/.vlicx-version"
REMOTE_SHA=""

if command -v curl >/dev/null 2>&1; then
    REMOTE_SHA=$(curl -s -m 5 "https://api.github.com/repos/cortane/Vlicx-Editor/commits/main" | grep '"sha"' | head -n 1 | cut -d '"' -f 4 || true)
fi

if [ -n "$REMOTE_SHA" ]; then
    echo "$REMOTE_SHA" > "$VERSION_FILE"
    echo "Recorded commit SHA: ${REMOTE_SHA:0:7}"
else
    date +%Y%m%d%H%M%S > "$VERSION_FILE"
fi

# Cleanup temp dir if created
if [ -n "$TMP_DIR" ] && [ -d "$TMP_DIR" ]; then
    rm -rf "$TMP_DIR"
fi

hash -r 2>/dev/null || true

echo ""
echo "\033[1;32m=======================================================\033[0m"
echo "\033[1;32m  Vlicx Editor installation completed successfully!     \033[0m"
echo "\033[1;32m=======================================================\033[0m"
echo "Installed binaries:"
echo "  - $PREFIX/bin/vlicx"
echo "  - $PREFIX/bin/vlicx-fo"
echo "  - $PREFIX/bin/vlicx-fi"
echo "Dictionary:"
echo "  - $JISYO_DST"
echo ""
