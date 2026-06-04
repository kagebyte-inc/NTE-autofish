# Autofish

[![CI / Build](https://github.com/kagebyte-inc/NTE-autofish/actions/workflows/ci.yml/badge.svg)](https://github.com/kagebyte-inc/NTE-autofish/actions/workflows/ci.yml)
[![Latest Release](https://img.shields.io/github/v/release/kagebyte-inc/NTE-autofish?label=release)](https://github.com/kagebyte-inc/NTE-autofish/releases/latest)

Desktop automation prototype for game fishing workflows.

## Stack

- C++20
- Qt 6 / Qt Quick / QML
- Python 3
- OpenCV

## Layout

- `src/` - Qt/C++ application shell and controller.
- `qml/` - Qt Quick UI.
- `python/autofish_vision/` - Python/OpenCV detection service.
- `python/autofish_vision/capture.py` - game-window lookup and screen capture.
- `python/autofish_vision/elements.py` - first-pass visual element analysis.

## Build UI

```sh
cmake -S . -B build
cmake --build build
./build/autofish
```

## Prebuilt releases (GitHub Actions)

Linux x86_64 builds are produced automatically by GitHub Actions on every push/PR and on every `v*` tag (build job runs inside an `archlinux:base-devel` container for fresh packages).

- Artifacts from CI runs are available on the [Actions tab](https://github.com/kagebyte-inc/NTE-autofish/actions).
- On tag push (e.g. `git tag v0.2.2 && git push origin v0.2.2`), a full release is created with a tarball attached. See [Releases](https://github.com/kagebyte-inc/NTE-autofish/releases).

The release tarball layout matches the `build/dist/` staging:

- `autofish` — dynamically linked Qt6 binary (requires a reasonably recent Qt6 runtime on the target system; built against current Arch Qt6)
- `python/autofish_vision/` + `requirements.txt`

### Using a prebuilt release

```sh
# extract
mkdir -p ~/opt/nte-autofish
cd ~/opt/nte-autofish
tar -xzf ~/Downloads/NTE-autofish-vX.Y.Z-linux-x86_64.tar.gz

# one-time Python deps setup (recommended: isolated venv)
python3 -m venv .venv
. .venv/bin/activate
pip install -r requirements.txt
deactivate

# run (the app will prefer the adjacent .venv for the vision service)
./autofish
```

If you already have the required packages globally available for your `python3`, you can skip the venv step — the binary falls back to `python3` from PATH (with `PYTHONPATH` set to include the local `python/`).

**Runtime dependencies on Linux (for prebuilts):** reasonably recent Qt 6 (base + declarative + wayland recommended), libxkbcommon, OpenGL libs.

Example on Ubuntu (may need newer backports or newer release for full 6.x features):

```sh
sudo apt install qt6-base qt6-declarative qt6-wayland libxkbcommon0 libgl1-mesa-glx
```

On Arch:

```sh
sudo pacman -S qt6-base qt6-declarative qt6-wayland libxkbcommon libglvnd
```

See the pipeline definition in [.github/workflows/ci.yml](.github/workflows/ci.yml).

## Run vision service

```sh
python -m venv .venv
. .venv/bin/activate
pip install -r requirements.txt
python -m autofish_vision.service --once --image path/to/frame.png
```

Use `PYTHONPATH=python` when running the module directly from the repository:

```sh
PYTHONPATH=python python -m autofish_vision.service --once --image path/to/frame.png
```

Watch the game window whose title contains `NTE`:

```sh
PYTHONPATH=python python -m autofish_vision.service --watch --window-title NTE
```

The service emits JSON events. `window_analyzed` includes the captured window geometry,
motion detector result, and detected visual regions such as bright or red elements.

List visible windows for GUI selection:

```sh
PYTHONPATH=python python -m autofish_vision.service --list-windows
```

Each window entry includes `id`, `title`, geometry, `pid`, `process_name`, and `icon_path`
when a matching desktop/theme icon can be resolved. For Proton/Wine games, pick the
entry by process name or actual window title in the GUI, then start vision on that exact
window id.

For backend testing without a real game window:

```sh
PYTHONPATH=python python -m autofish_vision.service --watch --synthetic
```

Use the Wayland portal picker:

```sh
PYTHONPATH=python python -m autofish_vision.service --watch --portal
```

This opens the system `xdg-desktop-portal` ScreenCast picker. After a window or monitor
is selected, frames are read from the returned PipeWire stream through GStreamer.

Write debug overlays for detector validation:

```sh
PYTHONPATH=python python -m autofish_vision.service --once --image docs/screenshots/nte_b4_fishing.png --debug-dir debug
PYTHONPATH=python python -m autofish_vision.service --watch --portal --debug-dir debug --debug-every 5 --interval 0.02
```

Current NTE fishing-prep analysis emits semantic elements such as `fishing_panel`,
`start_fishing_button`, `close_button`, `fish_card`, `rod_slot`, `bait_slot`, and
`water_roi`, followed by lower-level color regions for debugging.

Active fishing analysis currently targets the night-mode hook prompt. When the
center-top message appears, the analyzer emits `fish_hooked_prompt` and an estimated
`hook_action_button` region for the bottom-right interaction button.

Window enumeration and capture backends:

- X11/XWayland: install `xdotool` or `wmctrl`.
- Hyprland: `hyprctl` is used for window enumeration; install `grim` for region capture.
- Sway/wlroots: `swaymsg` is used for window enumeration; install `grim` for region capture.
- GNOME/KDE Wayland: global window enumeration and silent capture are intentionally
  restricted; these need an xdg-desktop-portal/PipeWire picker flow or an X11 session.

On Arch-based systems:

```sh
sudo pacman -S xdotool wmctrl grim xdg-desktop-portal gst-plugin-pipewire
```

Install the portal backend for your compositor/desktop as well:

```sh
sudo pacman -S xdg-desktop-portal-gnome
sudo pacman -S xdg-desktop-portal-kde
sudo pacman -S xdg-desktop-portal-hyprland
sudo pacman -S xdg-desktop-portal-wlr
```
