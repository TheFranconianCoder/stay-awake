# StayAwake

StayAwake is a performance-focused, lightweight Windows utility designed to manage system power states with surgical precision. While tools like "DontSleep" offer a wide array of complex features, StayAwake is built for users seeking an absolute minimalist approach—prioritizing a tiny memory footprint of ~1.4 MB and direct system interaction.

---

## 🛠 How it Works

StayAwake operates by interacting directly with the Windows Power Manager via the `SetThreadExecutionState` API. It prevents the system from entering sleep or turning off the display by flagging the current thread as required for system or display activity.

### Operating Modes

The application features two distinct modes, toggled by a **left-click** on the tray icon:

| Mode | Visual Indicator | Functionality |
| --- | --- | --- |
| **StayAwake** | **Red Screen** | Keeps the system and monitor active indefinitely (`ES_DISPLAY_REQUIRED`). |
| **Auto-Off** | **Dark Screen** | Allows the monitor to turn off after a specific idle period. |

In **Auto-Off** mode, the application monitors user activity via `GetLastInputInfo`. Once the idle time exceeds the configured limit, it broadcasts a `SC_MONITORPOWER` command to power down the displays.

### ⚡ Quick Actions

StayAwake supports rapid interaction through modifier keys:

| Action | Interaction | Functionality |
| --- | --- | --- |
| **Instant Off** | **Shift + Left-Click** | Immediately powers down all monitors without changing the current operating mode. |

---

## 📊 Visual Feedback (Tray Icon)

The tray icon is rendered dynamically using the Windows Graphics Device Interface (GDI) to avoid external dependencies.

* **Dynamic Progress**: In Auto-Off mode, a **Blue Bar** fills the monitor icon as the idle time approaches the limit.
* **Status Colors**: The screen remains **Red** when forced awake and turns **Dark** when the timer is active.
* **High-DPI Support**: StayAwake is "Per-Monitor DPI Aware V2". The icon is immediately redrawn without blurriness during scaling changes or monitor swaps (`WM_DPICHANGED`).

---

## 🛠 Technical Highlights

* **Hybrid Event Loop**: Uses `MsgWaitForMultipleObjects` to simultaneously listen for Windows messages and file system events. This ensures immediate reaction to configuration changes with zero CPU usage while waiting.
* **Atomic Configuration**: Settings are saved using a temporary-file-swap pattern to prevent data corruption during system crashes.
* **Resource Management**: In "StayAwake" mode, the internal heart-beat timer is fully deallocated (`KillTimer`) to eliminate unnecessary CPU cycles.
* **Instant Reload**: Utilizing `FindFirstChangeNotificationW`, StayAwake monitors its configuration directory and applies changes to the `.conf` file in real-time without a restart.

---

## ⚙️ Configuration

Settings are stored in a plain-text file:

`%LOCALAPPDATA%\StayAwake\stay_awake.conf`

**File Format**: Two space-separated integers:

1. **Mode**: `0` (StayAwake) or `1` (Auto-Off).
2. **Timeout**: Idle limit in seconds (default: `300`).

---

## 🚀 Deployment & Instance Management

### 📦 Installation via mise

StayAwake is optimized for use with **mise**:

```powershell
# Installation via mise
mise use -g github:TheFranconianCoder/stay-awake

```

### 🔄 Seamless Updates (Force Takeover)

StayAwake includes a **Single Instance Mutex** and a dedicated takeover logic:

1. It detects an existing instance via the `SA_CLASS` window class.
2. It sends a `WM_CLOSE` signal to the old instance to trigger a clean exit (removing the tray icon).
3. After a brief delay (`RESTART_DELAY_MS`), the new version takes over the tray position.

### 🔑 Automated Autostart

The application manages its own persistence. Upon every launch, it updates the `HKCU\...\Windows\CurrentVersion\Run` registry key to ensure the autostart always points to the current binary path—essential for path changes after a **mise** upgrade.

---

## 🏗 Build Instructions

The project is built using the **Zig** toolchain.

```powershell
zig build

```

The `build.zig` is configured to generate stripped, LTO-optimized binaries to maintain a minimal footprint.
