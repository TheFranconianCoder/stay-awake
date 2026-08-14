#include "power.h"

#include "config.h"
#include "tray.h"

// ===========================================================================
// Windows
// ===========================================================================
#ifdef _WIN32

static BOOL monitorIsOff = FALSE;

void applyPowerState(void) {
    SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED |
                            (globalMode == MODE_STAY_AWAKE ? ES_DISPLAY_REQUIRED : 0));

    if (globalMode == MODE_AUTO_OFF) {
        SetTimer(notifyData.hWnd, ID_TIMER_TICK, MS_PER_SEC, NULL);
    } else {
        KillTimer(notifyData.hWnd, ID_TIMER_TICK);
    }

    monitorIsOff = FALSE;
}

// ---------------------------------------------------------------------------
// Window procedure
// ---------------------------------------------------------------------------

// ReSharper disable once CppParameterMayBeConst
LRESULT CALLBACK wndProc(HWND hwnd, const UINT msg, const WPARAM wParam,
                         const LPARAM lParam) { // NOLINT(*-function-cognitive-complexity)
    switch (msg) {
    case WM_TRAYICON:
        if (lParam == WM_LBUTTONUP) {
            if (GetKeyState(VK_SHIFT) & 0x8000) {
                SendMessageW(HWND_BROADCAST, WM_SYSCOMMAND, SC_MONITORPOWER, 2);
                monitorIsOff = TRUE;
            } else {
                globalMode = globalMode == MODE_STAY_AWAKE ? MODE_AUTO_OFF : MODE_STAY_AWAKE;
                applyPowerState();
                saveConfig();
                updateTray(0);
            }
        } else if (lParam == WM_RBUTTONUP) {
            POINT mousePt;
            GetCursorPos(&mousePt);
            // ReSharper disable once CppLocalVariableMayBeConst
            HMENU hMenu = CreatePopupMenu();
            AppendMenuW(hMenu, MF_STRING, ID_TRAY_EXIT, L"Exit");
            SetForegroundWindow(hwnd);
            TrackPopupMenu(hMenu, TPM_RIGHTBUTTON, mousePt.x, mousePt.y, 0, hwnd, NULL);
            DestroyMenu(hMenu);
        }
        break;

    case WM_COMMAND:
        if (LOWORD(wParam) == ID_TRAY_EXIT) {
            DestroyWindow(hwnd);
        }
        break;

    case WM_TIMER:
        if (wParam == ID_TIMER_TICK) {
            LASTINPUTINFO inputInfo = {sizeof(LASTINPUTINFO)};
            if (GetLastInputInfo(&inputInfo)) {
                // ReSharper disable once CppLocalVariableMayBeConst
                DWORD64 currentTicks = GetTickCount64();
                if (currentTicks >= inputInfo.dwTime) {
                    const int IDLE = (int)((currentTicks - inputInfo.dwTime) / MS_PER_SEC);
                    if (IDLE >= 0 && globalMode == MODE_AUTO_OFF) {
                        updateTray(IDLE);
                        if (IDLE >= idleLimit && !monitorIsOff) {
                            SendMessageW(HWND_BROADCAST, WM_SYSCOMMAND, SC_MONITORPOWER, 2);
                            monitorIsOff = TRUE;
                        } else if (IDLE < idleLimit) {
                            monitorIsOff = FALSE;
                        }
                    }
                }
            }
        }
        break;

    case WM_DPICHANGED:
        updateTray(0);
        return 0;

    case WM_DESTROY:
        KillTimer(hwnd, ID_TIMER_TICK);
        SetThreadExecutionState(ES_CONTINUOUS);
        Shell_NotifyIconW(NIM_DELETE, &notifyData);
        if (notifyData.hIcon) {
            DestroyIcon(notifyData.hIcon);
            notifyData.hIcon = NULL;
        }
        PostQuitMessage(0);
        return 0;

    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}

#endif // _WIN32

// ===========================================================================
// Linux
// ===========================================================================
#ifdef __linux__

static gboolean monitorIsOff  = FALSE;
static guint32  inhibitCookie = 0;

// ---------------------------------------------------------------------------
// Inhibit display blanking via GNOME Session Manager D-Bus
// ---------------------------------------------------------------------------

static void startInhibit(void) {
    if (inhibitCookie > 0) {
        return; // already inhibiting
    }

    GError*     error = NULL;
    GDBusProxy* proxy =
        g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SESSION, G_DBUS_PROXY_FLAGS_NONE, NULL, "org.gnome.SessionManager",
                                      "/org/gnome/SessionManager", "org.gnome.SessionManager", NULL, &error);
    if (!proxy) {
        if (error) {
            g_error_free(error);
        }
        return;
    }

    // INHIBIT_IDLE = 8 — prevents screen blanking on GNOME
    GVariant* result =
        g_dbus_proxy_call_sync(proxy, "Inhibit", g_variant_new("(susu)", "StayAwake", 0, "Preventing display sleep", 8),
                               G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);
    if (result) {
        g_variant_get(result, "(u)", &inhibitCookie);
        g_variant_unref(result);
    }
    if (error) {
        g_error_free(error);
    }
    g_object_unref(proxy);
}

void stopInhibit(void) {
    if (inhibitCookie == 0) {
        return;
    }

    GError*     error = NULL;
    GDBusProxy* proxy =
        g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SESSION, G_DBUS_PROXY_FLAGS_NONE, NULL, "org.gnome.SessionManager",
                                      "/org/gnome/SessionManager", "org.gnome.SessionManager", NULL, &error);
    if (proxy) {
        g_dbus_proxy_call_sync(proxy, "Uninhibit", g_variant_new("(u)", inhibitCookie), G_DBUS_CALL_FLAGS_NONE, -1,
                               NULL, &error);
        g_object_unref(proxy);
    }
    if (error) {
        g_error_free(error);
    }
    inhibitCookie = 0;
}

// ---------------------------------------------------------------------------
// Idle time via Mutter D-Bus (proxy created once, reused)
// ---------------------------------------------------------------------------

static void ensureIdleProxy(void) {
    if (idleProxy) {
        return;
    }
    GError* error = NULL;
    idleProxy     = g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SESSION, G_DBUS_PROXY_FLAGS_NONE, NULL,
                                                  "org.gnome.Mutter.IdleMonitor", "/org/gnome/Mutter/IdleMonitor/Core",
                                                  "org.gnome.Mutter.IdleMonitor", NULL, &error);
    if (!idleProxy) {
        if (error) {
            g_error_free(error);
        }
    }
}

static guint64 queryIdleTimeMs(void) {
    ensureIdleProxy();
    if (!idleProxy) {
        return 0;
    }

    GError*   error  = NULL;
    GVariant* result = g_dbus_proxy_call_sync(idleProxy, "GetIdletime", NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);
    guint64   idleMs = 0;
    if (result) {
        GVariant* val = g_variant_get_child_value(result, 0);
        idleMs        = g_variant_get_uint64(val);
        g_variant_unref(val);
        g_variant_unref(result);
    }
    if (error) {
        g_error_free(error);
        // Proxy may be stale — reset it so next call recreates
        g_object_unref(idleProxy);
        idleProxy = NULL;
    }
    return idleMs;
}

// ---------------------------------------------------------------------------
// Turn off monitor via D-Bus (works on both X11 and Wayland)
// ---------------------------------------------------------------------------

void turnOffMonitor(void) {
    GError*     error = NULL;
    GDBusProxy* proxy =
        g_dbus_proxy_new_for_bus_sync(G_BUS_TYPE_SESSION, G_DBUS_PROXY_FLAGS_NONE, NULL, "org.gnome.ScreenSaver",
                                      "/org/gnome/ScreenSaver", "org.gnome.ScreenSaver", NULL, &error);
    if (proxy) {
        g_dbus_proxy_call_sync(proxy, "SetActive", g_variant_new("(b)", TRUE), G_DBUS_CALL_FLAGS_NONE, -1, NULL,
                               &error);
        g_object_unref(proxy);
    }
    if (error) {
        g_error_free(error);
        // Fallback to xset for X11 sessions
        error = NULL;
        g_spawn_command_line_async("xset dpms force off", &error);
        if (error) {
            g_error_free(error);
        }
    }
}

// ---------------------------------------------------------------------------
// Idle poll timer callback
// ---------------------------------------------------------------------------

static gboolean onIdlePoll(gpointer data) {
    (void)data;

    if (globalMode != MODE_AUTO_OFF) {
        return G_SOURCE_REMOVE;
    }

    guint64 idleMs = queryIdleTimeMs();
    int     idle   = (int)(idleMs / MS_PER_SEC);

    if (idle >= 0) {
        updateTray(idle);
        if (idle >= idleLimit && !monitorIsOff) {
            turnOffMonitor();
            monitorIsOff = TRUE;
        } else if (idle < idleLimit) {
            monitorIsOff = FALSE;
        }
    }
    return G_SOURCE_CONTINUE;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void stopIdlePolling(void) {
    if (idlePollSourceId > 0) {
        g_source_remove(idlePollSourceId);
        idlePollSourceId = 0;
    }
}

void startIdlePolling(void) {
    stopIdlePolling();
    if (globalMode == MODE_AUTO_OFF) {
        idlePollSourceId = g_timeout_add(LINUX_IDLE_POLL_MS, onIdlePoll, NULL);
    }
}

void applyPowerState(void) {
    monitorIsOff = FALSE;

    if (globalMode == MODE_STAY_AWAKE) {
        stopInhibit();
        startInhibit(); // re-acquire the inhibitor lock
        stopIdlePolling();
    } else {
        stopInhibit();
        startIdlePolling();
    }
}

#endif // __linux__
