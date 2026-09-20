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
color_print "\033[1;36m+--------------------------------------------------------------------------------+\033[0m\n"
color_print "\033[1;36m|\033[0m  \033[1;35mVLICX EDITOR\033[0m - \033[1;37mAuto Clean Installer & Updater        \033[0m                             \033[1;36m|\033[0m\n"
color_print "\033[1;36m|\033[0m  \033[2mLightweight & High-Performance C11 Terminal Editor\033[0m                            \033[1;36m|\033[0m\n"
color_print "\033[1;36m+--------------------------------------------------------------------------------+\033[0m\n\n"

INITIAL_DRAW=0

# Sleek Thin-Vertical Horizontal Gauge UI
draw_bar() {
    pct=$1
    text=$2
    cols=50
    filled=$((pct * cols / 100))
    empty=$((cols - filled))

    bar_fill=""
    i=0
    while [ $i -lt $filled ]; do
        bar_fill="${bar_fill}█"
        i=$((i + 1))
    done

    bar_empty=""
    i=0
    while [ $i -lt $empty ]; do
        bar_empty="${bar_empty}━"
        i=$((i + 1))
    done

    if [ "$INITIAL_DRAW" -eq 1 ]; then
        printf "\033[2A\r"
    fi
    INITIAL_DRAW=1

    printf "  \033[1;36mゲージ\033[0m     : \033[1;36m▕\033[1;32m%s\033[90m%s\033[1;36m▏\033[0m \033[1;33m%3d%%\033[0m \033[K\n" "$bar_fill" "$bar_empty" "$pct"
    printf "  \033[1;36mステータス\033[0m : \033[1;37m%-55s\033[0m \033[K\n" "$text"
}

# Step 1: Cleanup
draw_bar 10 "[1/4] 旧バイナリとキャッシュの削除中..."
rm -f /usr/local/bin/vlix* /usr/local/bin/vlicx* /usr/bin/vlix* /usr/bin/vlicx* /etc/profile.d/vlicx* >> "$LOG_FILE" 2>&1 || true
rm -rf "$HOME/.vlicx-jisyo" >> "$LOG_FILE" 2>&1 || true
hash -r >> "$LOG_FILE" 2>&1 || true
draw_bar 25 "[1/4] クリーンアップ完了"
sleep 0.2

# Step 2: Check Alpine dependencies
draw_bar 35 "[2/4] パッケージマネージャの検査中..."
if command -v apk >/dev/null 2>&1; then
    MISSING_PKGS=""
    for pkg in gcc musl-dev ncurses-dev make curl tar; do
        if ! apk info -e $pkg >/dev/null 2>&1; then
            MISSING_PKGS="$MISSING_PKGS $pkg"
        fi
    done
    if [ -n "$MISSING_PKGS" ]; then
        draw_bar 45 "[2/4] 不足パッケージを自動導入中 ($MISSING_PKGS)..."
        apk add --no-cache $MISSING_PKGS >> "$LOG_FILE" 2>&1
    fi
fi
draw_bar 50 "[2/4] 依存パッケージの準備完了"
sleep 0.2

# Step 3: Source acquisition
TMP_DIR=""
HERE=""

if [ -f "./src/main.c" ] && [ -f "./Makefile" ]; then
    HERE="$(pwd)"
    draw_bar 65 "[3/4] ローカルソースツリーを使用中"
else
    TMP_DIR="/tmp/vlicx-build-$$"
    mkdir -p "$TMP_DIR"
    draw_bar 60 "[3/4] 最新アーカイブのダウンロード中..."
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
    draw_bar 70 "[3/4] ソースコードの準備完了"
fi
sleep 0.2

# Step 4: Build and Install
cd "$HERE"
draw_bar 75 "[4/4] ビルドディレクトリの再構築中..."
make clean >> "$LOG_FILE" 2>&1

draw_bar 85 "[4/4] コンパイル中 (C11 + ncursesw)..."
make >> "$LOG_FILE" 2>&1

draw_bar 95 "[4/4] バイナリのインストール中 (/usr/local/bin)..."
make install PREFIX="$PREFIX" >> "$LOG_FILE" 2>&1

if [ -d /etc/profile.d ] && [ -f "bin/vlicx-login-check" ]; then
    cp -f bin/vlicx-login-check /etc/profile.d/vlicx-login-check.sh >> "$LOG_FILE" 2>&1 || true
    chmod +x /etc/profile.d/vlicx-login-check.sh >> "$LOG_FILE" 2>&1 || true
fi

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
color_print "\033[1;32m+--------------------------------------------------------------------------------+\033[0m\n"
color_print "\033[1;32m|  Vlicx Editor のインストールが完了しました                                      |\033[0m\n"
color_print "\033[1;32m+--------------------------------------------------------------------------------+\033[0m\n\n"
color_print "  \033[1;36mバイナリ位置\033[0m   : \033[1;37m$PREFIX/bin/vlicx\033[0m\n"
color_print "  \033[1;36mドキュメント\033[0m   : \033[1;37m$PREFIX/share/doc/vlicx/README_JA.md\033[0m\n"
color_print "  \033[1;36mフォルダ起動\033[0m   : \033[1;37mvlicx-fo <ディレクトリ>\033[0m\n"
color_print "  \033[1;36mファイル起動\033[0m   : \033[1;37mvlicx-fi <ファイル>\033[0m\n"
color_print "  \033[1;36mワンタッチ更新\033[0m : \033[1;37mvlicx-upd\033[0m\n"
color_print "  \033[1;36m処理所要時間\033[0m   : \033[1;33m$TIME_STR\033[0m\n\n"
