#!/bin/sh
# Vlicx Editor — Clean Installer & Updater
set -e

PREFIX="${1:-/usr/local}"
REPO_URL="https://github.com/cortane/Vlicx-Editor"
TAR_URL="https://github.com/cortane/Vlicx-Editor/archive/refs/heads/main.tar.gz"
LOG_FILE="/tmp/vlicx-install.log"

START_TIME=$(date +%s)

# Helper function for colored printf
color_print() {
    printf "%b" "$1"
}

# Clear log
> "$LOG_FILE"

# Draw Banner
clear 2>/dev/null || true
color_print "\n"
color_print "\033[1;36m+----------------------------------------------------------+\033[0m\n"
color_print "\033[1;36m|\033[0m  \033[1;35mVLICX EDITOR\033[0m - \033[1;37mAuto Clean Installer & Updater\033[0m          \033[1;36m|\033[0m\n"
color_print "\033[1;36m|\033[0m  \033[2mLightweight & High-Performance Terminal Editor\033[0m          \033[1;36m|\033[0m\n"
color_print "\033[1;36m+----------------------------------------------------------+\033[0m\n\n"

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
        bar="${bar}#"
        i=$((i + 1))
    done
    i=0
    while [ $i -lt $empty ]; do
        bar="${bar}-"
        i=$((i + 1))
    done

    printf "\r\033[1;36m[%s]\033[0m \033[1;32m%3d%%\033[0m \033[1;37m%s\033[0m \033[K" "$bar" "$pct" "$text"
}

# Step 1: Cleanup
color_print "\033[1;33m[1/4] クリーンアップ実行中...\033[0m\n"
draw_bar 10 "旧バイナリとキャッシュの削除中..."
rm -f /usr/local/bin/vlix* /usr/local/bin/vlicx* /usr/bin/vlix* /usr/bin/vlicx* >> "$LOG_FILE" 2>&1 || true
rm -rf "$HOME/.vlicx-jisyo" >> "$LOG_FILE" 2>&1 || true
hash -r >> "$LOG_FILE" 2>&1 || true
draw_bar 25 "クリーンアップ完了"
color_print "\n"
sleep 0.2

# Step 2: Check Alpine dependencies
color_print "\n\033[1;33m[2/4] 依存パッケージの確認...\033[0m\n"
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
color_print "\n"
sleep 0.2

# Step 3: Source acquisition
color_print "\n\033[1;33m[3/4] GitHub から最新ソースコード取得中...\033[0m\n"
TMP_DIR=""
HERE=""

if [ -f "./src/main.c" ] && [ -f "./Makefile" ]; then
    HERE="$(pwd)"
    draw_bar 65 "ローカルソースツリーを使用"
else
    TMP_DIR="/tmp/vlicx-build-$$"
    mkdir -p "$TMP_DIR"
    draw_bar 60 "最新アーカイブのダウンロード中..."
    curl -fsSL "$TAR_URL" | tar -xz -C "$TMP_DIR" >> "$LOG_FILE" 2>&1

    MAKEFILE_LOC=$(find "$TMP_DIR" -path "*/vlicx/Makefile" 2>/dev/null | head -n 1)
    if [ -z "$MAKEFILE_LOC" ]; then
        MAKEFILE_LOC=$(find "$TMP_DIR" -name "Makefile" 2>/dev/null | head -n 1)
    fi
    if [ -n "$MAKEFILE_LOC" ]; then
        HERE=$(dirname "$MAKEFILE_LOC")
    fi
    if [ -z "$HERE" ] || [ ! -f "$HERE/Makefile" ]; then
        color_print "\n\033[1;35mエラー: Makefile が見つかりませんでした。\033[0m\n"
        exit 1
    fi
    draw_bar 70 "ソースコードの準備完了"
fi
color_print "\n"
sleep 0.2

# Step 4: Build and Install
color_print "\n\033[1;33m[4/4] Vlicx のコンパイル & インストール中...\033[0m\n"
cd "$HERE"
draw_bar 75 "ビルドディレクトリの再構築中..."
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
draw_bar 100 "すべての処理が正常に完了しました"
color_print "\n\n"

# Calculate Elapsed Time
END_TIME=$(date +%s)
ELAPSED=$((END_TIME - START_TIME))
MINS=$((ELAPSED / 60))
SECS=$((ELAPSED % 60))

TIME_STR=""
if [ $MINS -gt 0 ]; then
    TIME_STR="${MINS}分${SECS}秒"
else
    TIME_STR="${SECS}秒"
fi

# Success Summary Card
color_print "\033[1;32m==========================================================\033[0m\n"
color_print "\033[1;32m  Vlicx Editor のインストールが完了しました\033[0m\n"
color_print "\033[1;32m==========================================================\033[0m\n\n"
color_print "  \033[1;36mバイナリ位置\033[0m   : \033[1;37m$PREFIX/bin/vlicx\033[0m\n"
color_print "  \033[1;36mフォルダ起動\033[0m   : \033[1;37mvlicx-fo <ディレクトリ>\033[0m\n"
color_print "  \033[1;36mファイル起動\033[0m   : \033[1;37mvlicx-fi <ファイル>\033[0m\n"
color_print "  \033[1;36mワンタッチ更新\033[0m : \033[1;37mvlicx-upd\033[0m\n"
color_print "  \033[1;36m処理所要時間\033[0m   : \033[1;33m$TIME_STR\033[0m\n\n"
