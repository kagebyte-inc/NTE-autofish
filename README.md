# Autofish

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
