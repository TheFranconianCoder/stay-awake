#pragma once

// ReSharper disable CppLocalVariableMayBeConst
// ReSharper disable CppParameterMayBeConst
// ReSharper disable CppRedundantCastExpression

#ifdef _WIN32
#include <windows.h>

#include <shellapi.h>
#endif

#ifdef __linux__
#include <fcntl.h>
#include <gio/gio.h>
#include <glib-unix.h>
#include <gtk/gtk.h>
#include <libayatana-appindicator/app-indicator.h>
#include <sys/file.h>
#include <sys/inotify.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------

enum {
    ID_TRAY_ICON              = 101,
    ID_TRAY_EXIT              = 102,
    ID_TIMER_TICK             = 1001,
    DEFAULT_IDLE_LIMIT        = 300,
    BIT_DEPTH_32              = 32,
    MS_PER_SEC                = 1000,
    RESTART_DELAY_MS          = 500,
    APP_VERSION_MAJOR         = 1,
    APP_VERSION_MINOR         = 2,
    APP_VERSION_PATCH         = 4,
    CONFIG_RELOAD_DEBOUNCE_MS = 250,
    LINUX_IDLE_POLL_MS        = 1000
};

#ifdef _WIN32
#define WM_TRAYICON (WM_USER + 1)
#endif

// ---------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------

typedef enum { MODE_STAY_AWAKE = 0, MODE_AUTO_OFF, MODE_COUNT } AppMode;

// ---------------------------------------------------------------------------
// Global state  (defined in main.c)
// ---------------------------------------------------------------------------

extern int     idleLimit;
extern AppMode globalMode;

#ifdef _WIN32
extern NOTIFYICONDATAW notifyData;
extern wchar_t         configPath[MAX_PATH];
extern wchar_t         configDir[MAX_PATH];
extern DWORD64         lastConfigLoad;
#endif

#ifdef __linux__
extern char          configPath[512];
extern char          configDir[512];
extern char          iconDir[512];
extern guint64       lastConfigLoad;
extern guint         idlePollSourceId;
extern int           inotifyFd;
extern int           inotifyWatchFd;
extern AppIndicator* indicator;
extern GDBusProxy*   idleProxy;
extern guint         signalWatchId;
#endif
