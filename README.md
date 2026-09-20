# Vlicx Editor — Lightweight C11 Terminal Text Editor

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)
[![Platform](https://img.shields.io/badge/Platform-Linux%20%7C%20Alpine-green.svg)]()

**Vlicx Editor** is an ultra-fast, lightweight, high-performance terminal text editor with an integrated dual-pane file explorer. Built with pure **C11** and **ncursesw**, Vlicx delivers zero-latency editing, low memory footprint, and intuitive keyboard navigation for Linux and Alpine Linux environments.

---

## ⚡ Quick One-Line Auto Installer & Updater

Run the following command on your Alpine Linux / Linux terminal to automatically install or update Vlicx Editor:

```bash
curl -fsSL https://raw.githubusercontent.com/cortane/Vlicx-Editor/main/install.sh | sh
```

---

## 🔥 Key Features

- **Blazing Fast**: Written in pure C11 with ncursesw. Zero overhead, near-instantaneous startup.
- **Integrated Explorer & Dual-Pane UI**: Split-view file explorer tree alongside a line-numbered text editor.
- **One-Click Auto Updater (`vlicx-upd`)**: Built-in update script to fetch and compile the latest release effortlessly.
- **Automatic Login Update Banner**: Alerts you upon SSH login whenever a new update is published on GitHub.
- **Convenient Launch Commands**:
  - `vlicx-fo <directory>`: Open folder in explorer mode.
  - `vlicx-fi <filepath>`: Open file directly in editor mode.
  - `vlicx-upd`: One-click system update.
- **i18n & Color Schemes**: Multi-language support (English / Japanese) and customizable color themes (Blue Console, Dark, Solar) saved in `~/.vlixrc`.
- **Keyboard Friendly**: Terminal XON/XOFF flow control managed safely (`Ctrl+S`, `Ctrl+Q` supported).

---

## 🚀 Usage Guide

```bash
vlicx-fo <folder_path>    # Open folder tree explorer
vlicx-fi <file_path>      # Open file directly in text editor
vlicx-upd                 # Update Vlicx Editor to latest GitHub version
```

### Examples
```bash
vlicx-fo /var/www/localhost/htdocs
vlicx-fi /etc/hosts
```

---

## ⌨️ Shortcuts & Keybindings

| Key Shortcut | Function | Description |
|---|---|---|
| **Ctrl+C** | Copy | Copy selected region, or current line |
| **Ctrl+V** | Paste | Paste clipboard buffer |
| **Ctrl+S** | Save | Save current file |
| **Ctrl+Q** | Exit | Quit editor |
| **Ctrl+K** | Clear Line | Clear text on current line |
| **Ctrl+L** | Focus Toggle | Switch focus between Explorer and Editor |
| **Alt+C** | Color Theme | Cycle themes (Blue Console / Dark / Solar) |
| **Alt+S** | Preferences | Open Settings modal (Language, Colors) |
| **Alt+T** | Search | Search text with wrap-around |
| **F5 / F6** | Selection | Start / End text selection range |
| **Delete** | Delete File | Delete selected item in Explorer (with prompt) |

---

## 🛠️ Build & Install from Source

### Dependencies (Alpine Linux)
```bash
apk add gcc musl-dev ncurses-dev make curl tar
```

### Manual Compilation
```bash
git clone https://github.com/cortane/Vlicx-Editor.git
cd Vlicx-Editor
make
make install
```

---

## 📄 License
Released under the [MIT License](LICENSE).
