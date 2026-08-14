# Commands

| Command | Description |
|---------|-------------|
| `task check` | Format + lint (clang-format, clang-tidy) |
| `task build` | Check + compile (zig build, ReleaseSmall) |
| `task build-linux` | Build for Linux |
| `task build-windows` | Cross-compile for Windows |
| `task run` | Build + launch binary |
| `task run-linux` | Build + launch on Linux |
| `task install-deps-linux` | Install Linux build dependencies |
| `task release VERSION=x.y.z` | Generate notes, tag, build, publish to GitHub |

# Conventions

- **Language:** C23, UNICODE (Windows), Win32 API (Windows), GTK3/GIO (Linux)
- **Naming:** `camelBack` functions/vars, `PascalCase` types, `UPPER_CASE` constants
- **Format:** LLVM style, 4-space indent, 120 col, Attach braces, left-aligned pointers
- **Lint:** clang-tidy with warnings-as-errors
- **Build:** Zig (build.zig); CMakeLists.txt for IDE analysis only
- **Tools:** mise manages zig, gh, cmake, ninja

# Stack

C → Zig → Win32 (Windows) / GTK3 + GIO + AppIndicator (Linux) → Task/mise

# Platform

- **Windows:** Win32 API, GDI icons, system tray via Shell_NotifyIcon
- **Linux:** GTK3, AppIndicator (libayatana-appindicator-glib), D-Bus (GNOME Session Manager, Mutter IdleMonitor, ScreenSaver), inotify
- **Cross-platform:** `#ifdef _WIN32` / `#ifdef __linux__` guards in shared source files
