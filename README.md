# StayAwake

StayAwake is a performance-focused, lightweight utility designed to manage system power states with surgical precision. While tools like "DontSleep" offer a wide array of complex features, StayAwake is built for users seeking an absolute minimalist approach — prioritizing a tiny memory footprint and direct system interaction.

Available for **Windows** and **Linux**.

---

## 🛠 How it Works

StayAwake interacts directly with the OS power management APIs to prevent the system from entering sleep or turning off the display.

### Operating Modes

The application features two distinct modes:

| Mode | Visual Indicator | Functionality |
| --- | --- | --- |
| **StayAwake** | **Red icon** | Keeps the system and monitor active indefinitely. |
| **Auto-Off** | **Dark icon with blue fill** | Allows the monitor to turn off after a specific idle period. Fills up in 10% steps as idle time increases. |

In **Auto-Off** mode, the application monitors user activity and once the idle time exceeds the configured limit, it powers down the displays.

### ⚡ Quick Actions

| Action | Interaction | Functionality |
| --- | --- | --- |
| **Toggle Mode** | **Right-click → Switch to ...** | Toggles between StayAwake and Auto-Off mode. |
| **Turn Off Monitor** | **Right-click → Turn Off Monitor** | Immediately blanks the display. |
| **Exit** | **Right-click → Exit** | Clean shutdown with inhibitor release. |

---

## 🖥 Platform Details

### Windows

* **Power Management**: `SetThreadExecutionState` API with `ES_CONTINUOUS`, `ES_SYSTEM_REQUIRED`, `ES_DISPLAY_REQUIRED`.
* **Idle Detection**: `GetLastInputInfo` with a 1-second timer tick.
* **Monitor Off**: `SC_MONITORPOWER` broadcast via `SendMessage`.
* **Tray Icon**: Dynamically rendered with GDI — a monitor silhouette with a blue progress bar in Auto-Off mode.
* **DPI Awareness**: Per-Monitor DPI Aware V2.
* **Autostart**: Self-manages `HKCU\...\Windows\CurrentVersion\Run` registry key.

### Linux

* **Power Management**: `org.gnome.SessionManager.Inhibit` D-Bus with `INHIBIT_IDLE` flag — prevents screen blanking on GNOME (X11 and Wayland).
* **Idle Detection**: D-Bus calls to `org.gnome.Mutter.IdleMonitor.GetIdletime` (GNOME Wayland compatible).
* **Monitor Off**: `org.gnome.ScreenSaver.SetActive(true)` D-Bus — blanks display without forced lock.
* **Tray Icon**: `libayatana-appindicator3` with 12 static PNG icons (11 auto-off fill levels + 1 awake). Auto-detects dark/light mode for correct outline color.
* **File Watching**: `GFileMonitor` for real-time config reload.
* **Autostart**: XDG `~/.config/autostart/stay-awake.desktop`.
* **Single Instance**: `flock()` on a pid file in `$XDG_RUNTIME_DIR`.

---

## ⚙️ Configuration

Settings are stored in a plain-text file:

* **Windows**: `%LOCALAPPDATA%\StayAwake\stay_awake.conf`
* **Linux**: `~/.config/stay-awake/stay_awake.conf`

**File Format**: Two space-separated integers:

1. **Mode**: `0` (StayAwake) or `1` (Auto-Off).
2. **Timeout**: Idle limit in seconds (default: `300`).

---

## 🚀 Deployment & Instance Management

### 📦 Installation via mise

StayAwake is optimized for use with **mise**:

```bash
mise use -g github:TheFranconianCoder/stay-awake
```

### 🔄 Seamless Updates (Force Takeover)

StayAwake enforces a single instance per platform:

* **Windows**: Named Mutex + `WM_CLOSE` to the old instance.
* **Linux**: `flock()` on `/tmp/stay-awake.pid`; old process is signaled via `SIGTERM`.

### 🔑 Automated Autostart

The application manages its own persistence on every launch:

* **Windows**: Updates the `HKCU\...\Windows\CurrentVersion\Run` registry key.
* **Linux**: Writes/updates `~/.config/autostart/stay-awake.desktop`.

---

## 🏗 Build Instructions

The project is built using the **Zig** toolchain.

### Prerequisites

**Windows**: No additional dependencies.

**Linux** (Ubuntu/Debian):

```bash
sudo apt install libgtk-3-dev libayatana-appindicator3-dev
```

### Build

```bash
zig build
```

Or via **mise** + **Task**:

```bash
task build
```

The `build.zig` is configured to generate stripped, LTO-optimized binaries to maintain a minimal footprint.

### Cross-compilation

```bash
# Build for Windows from Linux
zig build -Dtarget=x86_64-windows-gnu

# Build for Linux (native)
zig build
```
