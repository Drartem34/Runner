
<img width="3840" height="2160" alt="2026-09-13_22-55-47" src="https://github.com/user-attachments/assets/b14e4d33-cfb9-4fe8-95b5-0a38484f67d8" />
<img width="3840" height="2160" alt="2026-09-13_22-56-12" src="https://github.com/user-attachments/assets/67a97431-610f-472e-a568-4a0139d1a4d5" />


# Runner

*(🇺🇦 Українська версія нижче / Ukrainian version below)*

A sleek, glassmorphic application launcher and file searcher built for Wayland/Hyprland using GTK4 and C++. 

## Features
- **Instant App Search:** Search through your installed applications instantly.
- **File Search:** Type any file name and instantly search your entire system using the `locate` database.
- **Glassmorphic Design:** Native transparency and blur designed perfectly for Hyprland.
- **Lightweight:** Written in pure C++ using GTK4 layer shell.

## Controls
- **Typing:** Instantly search for applications or system files.
- **Arrow Keys (`Up`/`Down`/`Left`/`Right`):** Navigate through the grid or list of results.
- **Enter:** Launch the currently selected application or open the file.
- **Tab:** Manually expand or collapse the results grid.
- **Escape:** Close the launcher.

## Dependencies
Make sure you have the following installed on your system:
- `gtk4`
- `gtk4-layer-shell`
- `g++`, `make`, `pkgconf` / `pkg-config`
- `plocate` (or `mlocate`, for system-wide file search)
- `socat` (for IPC communication via socket)

## Installation

```bash
git clone https://github.com/Drartem34/Runner.git
cd Runner
```

You can either run the pre-compiled binary directly:
```bash
./runner
```

Or compile it yourself:
```bash
make
./runner
```

## Hyprland Integration
To seamlessly integrate Runner into Hyprland, get the beautiful background blur, and trigger it with a hotkey (e.g., `SUPER + SPACE`), add the following lines to your `hyprland.conf`:

```ini
# Enable background blur for the runner
layerrule = blur, runner
layerrule = ignorezero, runner

# Start the daemon in the background on startup (adjust the path to where your runner is)
exec-once = /path/to/Runner/runner

# Bind a hotkey to send the "show" signal to the IPC socket
bind = SUPER, SPACE, exec, echo "show" | socat - UNIX-CLIENT:~/.config/SYSui/runner.sock
```

---
---

# Runner (Українською)

Стильний "скляний" (glassmorphic) лаунчер додатків та пошуковик файлів, створений для Wayland/Hyprland на базі GTK4 та C++.

## Можливості
- **Миттєвий пошук:** Шукайте встановлені програми без затримок.
- **Пошук файлів:** Введіть назву будь-якого файлу, і лаунчер миттєво знайде його по всій системі завдяки базі `locate`.
- **Скляний дизайн:** Нативна прозорість та блюр, ідеально адаптовані для Hyprland.
- **Легкість:** Написано на чистому C++ з використанням GTK4 layer shell.

## Керування
- **Введення тексту:** Автоматичний пошук програм або файлів.
- **Стрілки (`Вгору`/`Вниз`/`Вліво`/`Вправо`):** Навігація по сітці або списку результатів.
- **Enter:** Запустити вибрану програму або відкрити файл.
- **Tab:** Розгорнути або згорнути список результатів вручну.
- **Escape:** Закрити лаунчер.

## Залежності
Переконайтеся, що у вас встановлені наступні пакети:
- `gtk4`
- `gtk4-layer-shell`
- `g++`, `make`, `pkgconf` / `pkg-config`
- `plocate` (або `mlocate`, для можливості пошуку файлів)
- `socat` (для взаємодії через сокет)

## Встановлення

```bash
git clone https://github.com/Drartem34/Runner.git
cd Runner
```

Ви можете запустити вже готовий скомпільований файл:
```bash
./runner
```

Або скомпілювати його самостійно:
```bash
make
./runner
```

## Інтеграція з Hyprland (Блюр та Гарячі клавіші)
Щоб лаунчер мав красиве розмиття (блюр) фону та відкривався по гарячій клавіші (наприклад, `SUPER + ПРОБІЛ`), додайте ці рядки у ваш файл конфігурації `hyprland.conf`:

```ini
# Вмикаємо блюр (розмиття) фону для лаунчера
layerrule = blur, runner
layerrule = ignorezero, runner

# Запускаємо процес у фоні при старті системи (замініть шлях на реальну папку з runner)
exec-once = /шлях/до/папки/Runner/runner

# Біндимо гарячу клавішу для відкриття лаунчера
bind = SUPER, SPACE, exec, echo "show" | socat - UNIX-CLIENT:~/.config/SYSui/runner.sock
```
