# Vlicx Editor — 超軽量 C11 ターミナル・テキストエディタ

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20Alpine-green.svg)]()

**Vlicx Editor** は、Pure C11 と ncursesw で構築された超高速・軽量な Linux / Alpine Linux 向けターミナルテキストエディタです。2画面分割ファイルエクスプローラーを内蔵し、最小限のメモリ消費と爆速の起動速度を実現します。

---

## ⚡ 1行ワンタッチ自動インストール & 更新

Alpine Linux や Linux のターミナルで以下の1行を実行するだけで、インストールおよび最新版へのアップデートが完了します。

```bash
curl -fsSL https://raw.githubusercontent.com/cortane/Vlicx-Editor/main/install.sh | sh
```

---

## 🔥 主な特徴

- **圧倒的な爆速動作**: Pure C11 + ncursesw 実装。ガベージコレクションなし、超低メモリ消費。
- **2画面エクスプローラー統合**: 左側に折りたたみ可能なディレクトリツリー、右側にエディタを配置。
- **ワンタッチ自動更新 (`vlicx-upd`)**: コマンド1つで最新コードを取得して自動コンパイル・更新。
- **ログイン時自動アップデート通知**: SSH接続時（ログイン時）、GitHub上に新バージョンがあれば自動で案内バナーを表示。
- **便利な起動コマンド**:
  - `vlicx-fo <フォルダパス>`: フォルダエクスプローラーモードで起動
  - `vlicx-fi <ファイルパス>`: ファイル直接編集モードで起動
  - `vlicx-upd`: システム全自動アップデート
- **日本語 / 英語 & カラーテーマ対応**: 多言語UI表示、および3種類の配色テーマ（Blue Console, Dark, Solar）を `~/.vlixrc` に自動保存。
- **安全なフロー制御**: XON/XOFF フロー制御を自動無効化し、`Ctrl+S` (保存) や `Ctrl+Q` (終了) を安全に使用可能。

---

## 🚀 使い方

```bash
vlicx-fo <フォルダパス>    # フォルダツリーを開く
vlicx-fi <ファイルパス>    # ファイルを直接開く
vlicx-upd                 # Vlicx Editor を最新版に一発更新
```

### 実行例
```bash
vlicx-fo /var/www/localhost/htdocs
vlicx-fi /etc/hosts
```

---

## ⌨️ ショートカットキー一覧

| キー操作 | 機能 | 詳細 |
|---|---|---|
| **Ctrl+C** | コピー | 選択範囲、または現在行をコピー |
| **Ctrl+V** | 貼り付け | クリップボードから貼り付け |
| **Ctrl+S** | 保存 | ファイルを上書き保存 |
| **Ctrl+Q** | 終了 | エディタを終了 |
| **Ctrl+K** | 行クリア | カーソル行のテキストを全削除 |
| **Ctrl+L** | フォーカス切替 | エクスプローラー ↔ エディタ の操作切り替え |
| **Alt+C** | テーマ変更 | カラー配色を切り替え (Blue Console / Dark / Solar) |
| **Alt+S** | 設定画面 | 言語設定（日本語/英語）・配色変更モーダルを開く |
| **Alt+T** | 検索 | 選択テキストの検索（ラップアラウンド検索対応） |
| **F5 / F6** | 選択モード | 範囲選択の開始 (F5) / 終了 (F6) |
| **Delete** | ファイル削除 | エクスプローラー選択中のファイル・フォルダを削除（確認あり） |

---

## 🛠️ ソースコードからのビルド

### 依存パッケージの導入 (Alpine Linux)
```bash
apk add gcc musl-dev ncurses-dev make curl tar
```

### 手動コンパイル手順
```bash
git clone https://github.com/cortane/Vlicx-Editor.git
cd Vlicx-Editor
make
make install
```

---

## 📄 ライセンス
[MIT License](LICENSE) のもとで公開されています。
