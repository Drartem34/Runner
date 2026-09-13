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

## Controls
- **Typing:** Instantly search for applications or system files.
- **Arrow Keys (`Up`/`Down`/`Left`/`Right`):** Navigate through the grid or list of results.
- **Enter:** Launch the currently selected application or open the file.
- **Tab:** Manually expand or collapse the results grid.
- **Escape:** Close the launcher.

## Hyprland Integration
To seamlessly integrate Runner into Hyprland and trigger it with a hotkey (e.g., `SUPER + SPACE`), add the following lines to your `hyprland.conf`:

```ini
# Start the daemon in the background on startup
exec-once = /path/to/Runner/runner

# Bind a hotkey to send the "show" signal to the IPC socket
bind = SUPER, SPACE, exec, echo "show" | socat - UNIX-CLIENT:~/.config/SYSui/runner.sock
```
*(Make sure to replace `/path/to/Runner/runner` with the actual path to your compiled binary or its installation location, and ensure you have `socat` installed).*
