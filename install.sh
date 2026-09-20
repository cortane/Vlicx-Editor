#!/bin/sh
# Vlicx Editor — Stylish Auto Clean Installer & Updater
# Usage:
#   curl -fsSL https://raw.githubusercontent.com/cortane/Vlicx-Editor/main/install.sh | sh

set -e

PREFIX="${1:-/usr/local}"
REPO_URL="https://github.com/cortane/Vlicx-Editor"
TAR_URL="https://github.com/cortane/Vlicx-Editor/archive/refs/heads/main.tar.gz"
LOG_FILE="/tmp/vlicx-install.log"

# ANSI Color Tokens
CYAN='\033[1;36m'
MAGENTA='\033[1;35m'
GREEN='\033[1;32m'
YELLOW='\033[1;33m'
WHITE='\033[1;37m'
BLUE='\033[1;34m'
DIM='\033[2m'
RESET='\033[0m'

# Clear log
> "$LOG_FILE"

# Draw Banner
clear 2>/dev/null || true
echo ""
echo "${CYAN}╭──────────────────────────────────────────────────────────╮${RESET}"
echo "${CYAN}│${RESET}  ${MAGENTA}🚀  VLICX EDITOR${RESET}  — ${WHITE}Auto Clean Installer & Updater${RESET}   ${CYAN}│${RESET}"
echo "${CYAN}│${RESET}  ${DIM}Lightweight & High-Performance Terminal Editor${RESET}          ${CYAN}│${RESET}"
echo "${CYAN}╰──────────────────────────────────────────────────────────╯${RESET}"
echo ""

# Progress bar function
draw_bar() {
    pct=$1
    text=$2
    cols=30
    filled=$((pct * cols / 100))
    empty=$((cols - filled))

    bar=""
    i=0
    while [ $i -lt $filled ]; do
        bar="${bar}█"
        i=$((i + 1))
    done
    i=0
    while [ $i -lt $empty ]; do
        bar="${bar}░"
        i=$((i + 1))
    done

    printf "\r${CYAN}[%s]${RESET} ${GREEN}%3d%%${RESET} ${WHITE}%s${RESET} \033[K" "$bar" "$pct" "$text"
}

# Step 1: Cleanup
echo "${YELLOW}▶ [1/4] 🧹 古いバイナリ・キャッシュの清掃中...${RESET}"
draw_bar 10 "旧バイナリとキャッシュの削除..."
rm -f /usr/local/bin/vlix* /usr/local/bin/vlicx* /usr/bin/vlix* /usr/bin/vlicx* >> "$LOG_FILE" 2>&1 || true
rm -rf "$HOME/.vlicx-jisyo" >> "$LOG_FILE" 2>&1 || true
hash -r >> "$LOG_FILE" 2>&1 || true
draw_bar 25 "クリーンアップ完了"
echo ""
sleep 0.2

# Step 2: Check Alpine dependencies
echo ""
echo "${YELLOW}▶ [2/4] 📦 ビルド環境・依存パッケージ確認...${RESET}"
draw_bar 35 "パッケージマネージャの検査中..."
if command -v apk >/dev/null 2>&1; then
    MISSING_PKGS=""
    for pkg in gcc musl-dev ncurses-dev make curl tar; do
        if ! apk info -e $pkg >/dev/null 2>&1; then
            MISSING_PKGS="$MISSING_PKGS $pkg"
        fi
    done
    if [ -n "$MISSING_PKGS" ]; then
        draw_bar 45 "不足パッケージを自動導入中 ($MISSING_PKGS)..."
        apk add --no-cache $MISSING_PKGS >> "$LOG_FILE" 2>&1
    fi
fi
draw_bar 50 "依存パッケージの準備完了"
echo ""
sleep 0.2

# Step 3: Source acquisition
echo ""
echo "${YELLOW}▶ [3/4] 🌐 GitHub から最新ソースコードの取得...${RESET}"
TMP_DIR=""
HERE=""

if [ -f "./src/main.c" ] && [ -f "./Makefile" ]; then
    HERE="$(pwd)"
    draw_bar 65 "ローカルソースツリーを使用"
else
    TMP_DIR="/tmp/vlicx-build-$$"
    mkdir -p "$TMP_DIR"
    draw_bar 60 "最新アーカイブをダウンロード中..."
    curl -fsSL "$TAR_URL" | tar -xz -C "$TMP_DIR" >> "$LOG_FILE" 2>&1

    MAKEFILE_LOC=$(find "$TMP_DIR" -path "*/vlicx/Makefile" 2>/dev/null | head -n 1)
    if [ -z "$MAKEFILE_LOC" ]; then
        MAKEFILE_LOC=$(find "$TMP_DIR" -name "Makefile" 2>/dev/null | head -n 1)
    fi
    if [ -n "$MAKEFILE_LOC" ]; then
        HERE=$(dirname "$MAKEFILE_LOC")
    fi
    if [ -z "$HERE" ] || [ ! -f "$HERE/Makefile" ]; then
        echo ""
        echo "${MAGENTA}エラー: ダウンロードしたアーカイブ内に Makefile が見つかりませんでした。${RESET}"
        exit 1
    fi
    draw_bar 70 "ソースコードの準備完了"
fi
echo ""
sleep 0.2

# Step 4: Build and Install
echo ""
echo "${YELLOW}▶ [4/4] ⚡ Vlicx のコンパイル＆インストール中...${RESET}"
cd "$HERE"
draw_bar 75 "古いオブジェクトファイルのクリーンアップ..."
make clean >> "$LOG_FILE" 2>&1

draw_bar 85 "コンパイル中 (C11 + ncursesw)..."
make >> "$LOG_FILE" 2>&1

draw_bar 95 "バイナリのインストール中 (/usr/local/bin)..."
make install PREFIX="$PREFIX" >> "$LOG_FILE" 2>&1

# Record version
VERSION_FILE="$HOME/.vlicx-version"
if command -v curl >/dev/null 2>&1; then
    REMOTE_SHA=$(curl -s -m 5 "https://api.github.com/repos/cortane/Vlicx-Editor/commits/main" | grep '"sha"' | head -n 1 | cut -d '"' -f 4 || true)
fi

if [ -n "$REMOTE_SHA" ]; then
    echo "$REMOTE_SHA" > "$VERSION_FILE"
else
    date +%Y%m%d%H%M%S > "$VERSION_FILE"
fi

if [ -n "$TMP_DIR" ] && [ -d "$TMP_DIR" ]; then
    rm -rf "$TMP_DIR"
fi

hash -r >> "$LOG_FILE" 2>&1 || true
draw_bar 100 "すべての処理が完了しました！"
echo ""
echo ""

# Success Summary Card
echo "${GREEN}✨ ────────────────────────────────────────────────────────── ✨${RESET}"
echo "${GREEN}   🎉  VLICX EDITOR HAS BEEN SUCCESSFULLY INSTALLED!         ${RESET}"
echo "${GREEN}✨ ────────────────────────────────────────────────────────── ✨${RESET}"
echo ""
echo "   ${CYAN}📍 バイナリ位置${RESET} : ${WHITE}$PREFIX/bin/vlicx${RESET}"
echo "   ${CYAN}📁 フォルダ起動${RESET} : ${WHITE}vlicx-fo <ディレクトリ>${RESET}"
echo "   ${CYAN}📄 ファイル起動${RESET} : ${WHITE}vlicx-fi <ファイル>${RESET}"
echo ""
echo "   ${MAGENTA}🚀 準備完了！ vlicx-fo コマンドでエディタをお楽しみください。${RESET}"
echo ""
