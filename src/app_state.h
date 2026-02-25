#pragma once

// ReSharper disable CppLocalVariableMayBeConst
// ReSharper disable CppParameterMayBeConst
// ReSharper disable CppRedundantCastExpression

#include <windows.h>

#include <shellapi.h>

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
    APP_VERSION_PATCH         = 1,
    CONFIG_RELOAD_DEBOUNCE_MS = 100
};

#define WM_TRAYICON (WM_USER + 1)

// ---------------------------------------------------------------------------
// Types
// ---------------------------------------------------------------------------

typedef enum { MODE_STAY_AWAKE = 0, MODE_AUTO_OFF, MODE_COUNT } AppMode;

// ---------------------------------------------------------------------------
// Global state  (defined in main.c)
// ---------------------------------------------------------------------------

extern int             idleLimit;
extern AppMode         mode;
extern BOOL            monitorIsOff;
extern NOTIFYICONDATAW notifyData;
extern wchar_t         configPath[MAX_PATH];
extern wchar_t         configDir[MAX_PATH];
extern DWORD64         lastConfigLoad;