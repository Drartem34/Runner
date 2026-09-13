# Runner

A sleek, glassmorphic application launcher and file searcher built for Wayland/Hyprland using GTK4 and C++. 

## Features
- **Instant App Search:** Search through your installed applications instantly.
- **File Search:** Type any file name and instantly search your entire system using the `locate` database.
- **Glassmorphic Design:** Native transparency and blur designed perfectly for Hyprland.
- **Lightweight:** Written in pure C++ using GTK4 layer shell.
- **Keyboard & Mouse Support:** Navigate with arrow keys, click or hit Enter to launch.

## Dependencies
Make sure you have the following installed on your system:
- `gtk4`
- `gtk4-layer-shell`
- `pkgconf` / `pkg-config`
- `g++` 
- `plocate` (or `mlocate`, for file search capability)

## Installation

```bash
git clone https://github.com/Drartem34/Runner.git
cd Runner
make
```

To run it directly:
```bash
./runner
```

## Usage
Simply run `runner` from your terminal or bind it to a hotkey in your window manager (e.g., Hyprland).

To trigger the launcher via IPC socket (for example, via a hotkey), you can send the "show" command to its socket:
```bash
echo "show" | socat - UNIX-CLIENT:~/.config/SYSui/runner.sock
```
