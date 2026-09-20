# Vlix（ヴィリック）— C Edition

Linuxライクな操作感を持つ、フォルダ/ファイルエクスプローラー内蔵のターミナルテキストエディタ。
Pure C + ncurses で実装。外部ライブラリ不要（ncurses のみ）。

## ビルド（Alpine Linux）

```sh
apk add gcc musl-dev ncurses-dev make
make            # 通常ビルド
make static     # 静的リンク（ncurses-static も必要: apk add ncurses-static）
```

## インストール

```sh
./install.sh              # /usr/local にインストール
./install.sh ~/.local     # カスタムプレフィックス
```

## 使い方

```sh
vlix-fo <フォルダパス>    # フォルダをExplorerで開く
vlix-fi <ファイルパス>    # ファイルをエディタで直接開く
```

相対パス・絶対パス両対応：
```sh
vlix-fo FFGm
vlix-fo /root/html
vlix-fi GRJs
vlix-fi /root/css/Style.css
```

## 画面構成

```
+---------------------------------------------------------+
|              基本操作一覧（固定・横幅いっぱい）           |
+----------------------+------------------------------------+
|  フォルダ内容表示     |                                    |
|  （Explorer）         |             エディタ                |
|  ・開閉式フォルダ      |          行番号 + テキスト          |
|  ・矢印キーで選択      |                                    |
+----------------------+------------------------------------+
|  ステータスバー（ファイル名・カーソル位置・メッセージ）    |
+---------------------------------------------------------+
```

## ショートカット一覧

| キー | 機能 | 補足 |
|---|---|---|
| Ctrl+C | Copy | 選択範囲、なければ現在行 |
| Ctrl+S | Save | XON/XOFF 自動無効化済み |
| Ctrl+K | Line Clear | 現在行を空に |
| Ctrl+V | Paste | |
| Ctrl+Q | Exit | XON/XOFF 自動無効化済み |
| Alt+C | Color Scheme | 3種類を循環（設定保存） |
| Ctrl+L | Console Move | Explorer↔エディタ切替 |
| Alt+T | Search | 選択テキストで検索（ラップアラウンド） |
| F5 | Selection Mode | 選択開始（Ctrl+1 対応端末あり） |
| F6 | End Selection | 選択終了、選択は維持 |
| Alt+S | Vlix Setting | 言語・配色変更（設定ファイル保存） |
| Ctrl+E | Error Display | |
| Alt+H | Help | Ctrl+H=BS衝突回避のためAlt+H |
| Delete | Delete | Explorer時のみ、確認あり |

### Explorer操作

- `↑` `↓`：選択移動
- `→` / `Enter`：フォルダ展開 / ファイルをエディタで開く
- `←` / `Enter`（展開中フォルダ）：折りたたみ
- `Delete`：選択中を削除（確認ダイアログ表示）

## 設定ファイル

`~/.vlixrc` に自動保存：
```ini
color_scheme=0
language=0
```

- `color_scheme`: 0=Blue Console, 1=Dark, 2=Solar
- `language`: 0=English, 1=Japanese

Alt+S の設定画面から変更可能。変更は即座に保存され、再起動後も維持。

## 言語設定

Alt+S → "Change language" で English / 日本語 を切り替え。
全UIメッセージ（ステータスバー、オーバーレイ、ショートカットラベル）が切り替わる。

## ディレクトリ構成

```
vlicx/
├── Makefile
├── install.sh
├── README.md
├── bin/
│   ├── vlix-fo
│   └── vlix-fi
└── src/
    ├── vlix.h        # 共有ヘッダ（全型・定数・宣言）
    ├── main.c        # エントリポイント・メインループ・入力処理
    ├── editor.c      # テキストエディタ（行管理・編集・検索）
    ├── explorer.c    # ファイルエクスプローラー（ツリー管理）
    ├── ui.c          # ncurses描画（パネル・オーバーレイ・ステータス）
    └── config.c      # i18n文字列テーブル・設定永続化・配色
```

## Python版からの改善点

| 項目 | Python版 | C版 |
|---|---|---|
| 速度 | curses + GC | 直接ncurses、GCなし |
| 選択モード | Ctrl+F1/F2（非対応） | F5/F6 + Ctrl+1/2（対応端末） |
| 設定保存 | なし | ~/.vlixrc に永続化 |
| XON/XOFF | 手動 stty -ixon | 自動無効化（tcsetattr） |
| Ctrl+H衝突 | 未解決 | Alt+H に変更で回避 |
| 検索 | 下方向のみ | ラップアラウンド（双方向） |
| 削除確認 | なし | 確認ダイアログ (y/N) |
| 言語 | 日本語のみ | EN/JP 切り替え |
| 行番号 | なし | エディタ左端に表示 |
| 依存 | Python3 | gcc + ncurses のみ |

## 既知の制限

- **Ctrl+1/Ctrl+2**: 端末によっては送信されない。F5/F6 がフォールバック。
- **UTF-8入力**: 日本語のIME入力は端末依存。表示（ファイルから読み込み）は対応。
- **長い行**: 8191バイトで分割される可能性あり。
- **tmux/screen**: raw() モードの動作が異なる場合あり。
