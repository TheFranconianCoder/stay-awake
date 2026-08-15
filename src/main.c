#include "app_state.h"
#include "config.h"
#include "power.h"
#include "tray.h"

// ---------------------------------------------------------------------------
// Global definitions  (declared extern in app_state.h)
// ---------------------------------------------------------------------------

int     idleLimit  = DEFAULT_IDLE_LIMIT;
AppMode globalMode = MODE_STAY_AWAKE;

#ifdef _WIN32
NOTIFYICONDATAW notifyData = {0};
wchar_t         configPath[MAX_PATH];
wchar_t         configDir[MAX_PATH];
DWORD64         lastConfigLoad = 0;
#endif

#ifdef __linux__
char          configPath[512];
char          configDir[512];
char          iconDir[512];
guint64       lastConfigLoad   = 0;
guint         idlePollSourceId = 0;
GFileMonitor* configMonitor    = NULL;
AppIndicator* indicator        = NULL;
GtkMenuItem*  toggleMenuItem   = NULL;
GDBusProxy*   idleProxy        = NULL;
guint         signalWatchId    = 0;
#endif

// ===========================================================================
// Windows
// ===========================================================================
#ifdef _WIN32

// ---------------------------------------------------------------------------
// DPI awareness
// ---------------------------------------------------------------------------

static void enableDpiAwareness(void) {
    const HMODULE H_USER32 = GetModuleHandleW(L"user32.dll");
    if (H_USER32) {
        typedef BOOL(WINAPI * SetProcessDpiAwarenessContextProc)(DPI_AWARENESS_CONTEXT);
        // ReSharper disable once CppLocalVariableMayBeConst
        SetProcessDpiAwarenessContextProc setDpiContext =
            (SetProcessDpiAwarenessContextProc)GetProcAddress(H_USER32, "SetProcessDpiAwarenessContext");
        if (setDpiContext) {
            setDpiContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
            return;
        }
    }
    SetProcessDPIAware();
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

int main(void) { // NOLINT(*-function-cognitive-complexity)
    enableDpiAwareness();

    HANDLE hMutex = CreateMutexW(NULL, TRUE, L"StayAwake_SingleInstance_Mutex");
    if (!hMutex) {
        return 1;
    }

    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        // ReSharper disable once CppLocalVariableMayBeConst
        HWND oldWnd = FindWindowW(L"SA_CLASS", NULL);
        if (oldWnd) {
            SendMessageW(oldWnd, WM_CLOSE, 0, 0);
            Sleep(RESTART_DELAY_MS);
        }
        CloseHandle(hMutex);
        hMutex = CreateMutexW(NULL, TRUE, L"StayAwake_SingleInstance_Mutex");
        if (!hMutex || GetLastError() == ERROR_ALREADY_EXISTS) {
            if (hMutex) {
                CloseHandle(hMutex);
            }
            return 1;
        }
    }

    initConfigPath();
    loadConfig();
    updateAutostartIfNeeded();

    const WNDCLASSEXW WIN_CLASS = {
        sizeof(WNDCLASSEXW), 0, wndProc, 0, 0, GetModuleHandle(NULL), NULL, NULL, NULL, NULL, L"SA_CLASS", NULL};
    RegisterClassExW(&WIN_CLASS);

    // ReSharper disable once CppLocalVariableMayBeConst
    HWND hwnd = CreateWindowExW(0, L"SA_CLASS", NULL, 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, NULL, NULL);

    notifyData.cbSize           = sizeof(NOTIFYICONDATAW);
    notifyData.hWnd             = hwnd;
    notifyData.uID              = ID_TRAY_ICON;
    notifyData.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    notifyData.uCallbackMessage = WM_TRAYICON;
    notifyData.hIcon            = createDynamicIcon(0, globalMode);
    wcscpy_s(notifyData.szTip, 128, L"StayAwake");

    if (!Shell_NotifyIconW(NIM_ADD, &notifyData)) {
        if (notifyData.hIcon) {
            DestroyIcon(notifyData.hIcon);
        }
        CloseHandle(hMutex);
        return 1;
    }

    applyPowerState();
    updateTooltip();

    HANDLE hNotify = FindFirstChangeNotificationW(configDir, FALSE, FILE_NOTIFY_CHANGE_LAST_WRITE);
    if (hNotify == INVALID_HANDLE_VALUE) {
        hNotify = NULL;
    }

    MSG msg;

    while (TRUE) {
        HANDLE waitHandles[2];
        DWORD  handleCount = 0;
        if (hNotify) {
            waitHandles[handleCount++] = hNotify;
        }

        // ReSharper disable once CppLocalVariableMayBeConst
        DWORD dwWait = MsgWaitForMultipleObjects(handleCount, waitHandles, FALSE, INFINITE, QS_ALLINPUT);

        if (dwWait == WAIT_OBJECT_0 && handleCount > 0) {
            // ReSharper disable once CppLocalVariableMayBeConst
            DWORD64 now = GetTickCount64();
            if (now - lastConfigLoad > CONFIG_RELOAD_DEBOUNCE_MS) {
                loadConfig();
                applyPowerState();
                updateTray(0);
                lastConfigLoad = now;
            }
            FindNextChangeNotification(hNotify);
        }

        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                goto cleanup;
            }
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

cleanup:
    if (hNotify) {
        FindCloseChangeNotification(hNotify);
    }
    CloseHandle(hMutex);
    return 0;
}

#endif // _WIN32

// ===========================================================================
// Linux
// ===========================================================================
#ifdef __linux__

// ---------------------------------------------------------------------------
// GFileMonitor callback — config file changed
// ---------------------------------------------------------------------------

static void onConfigChanged(GFileMonitor* monitor, GFile* file, GFile* otherFile, GFileMonitorEvent event,
                            gpointer data) {
    (void)monitor;
    (void)otherFile;
    (void)data;

    if (event != G_FILE_MONITOR_EVENT_CHANGED && event != G_FILE_MONITOR_EVENT_CREATED &&
        event != G_FILE_MONITOR_EVENT_MOVED_IN) {
        return;
    }

    // Only react to the config file itself
    gchar*   baseName = g_file_get_basename(file);
    gboolean isConfig = g_strcmp0(baseName, CONFIG_FILENAME) == 0;
    g_free(baseName);
    if (!isConfig) {
        return;
    }

    guint64 now = g_get_monotonic_time() / 1000;
    if (now - lastConfigLoad > CONFIG_RELOAD_DEBOUNCE_MS) {
        loadConfig();
        applyPowerState();
        updateTray(0);
        lastConfigLoad = now;
    }
}

// ---------------------------------------------------------------------------
// GtkMenu callbacks for AppIndicator
// ---------------------------------------------------------------------------

static void onToggleMode(GtkMenuItem* item,
                         gpointer     userData) { // NOLINT(*-function-cognitive-complexity)
    (void)item;
    (void)userData;
    globalMode = globalMode == MODE_STAY_AWAKE ? MODE_AUTO_OFF : MODE_STAY_AWAKE;
    applyPowerState();
    saveConfig();
    updateTray(0);
}

static void onMonitorOff(GtkMenuItem* item,
                         gpointer     userData) { // NOLINT(*-function-cognitive-complexity)
    (void)item;
    (void)userData;
    turnOffMonitor();
}

// ---------------------------------------------------------------------------
// Cleanup on exit
// ---------------------------------------------------------------------------

static void cleanup(void) {
    stopIdlePolling();

    if (signalWatchId > 0) {
        g_source_remove(signalWatchId);
        signalWatchId = 0;
    }

    if (idleProxy) {
        g_object_unref(idleProxy);
        idleProxy = NULL;
    }

    stopInhibit();

    if (indicator) {
        g_object_unref(indicator);
        indicator = NULL;
    }

    if (configMonitor) {
        g_file_monitor_cancel(configMonitor);
        g_object_unref(configMonitor);
        configMonitor = NULL;
    }
}

// ---------------------------------------------------------------------------
// SIGTERM handler (single-instance restart)
// ---------------------------------------------------------------------------

static gboolean onSigTerm(gpointer data) {
    (void)data;
    signalWatchId = 0; // source is destroyed after returning G_SOURCE_REMOVE
    gtk_main_quit();
    return G_SOURCE_REMOVE;
}

// ---------------------------------------------------------------------------
// Single instance via flock
// ---------------------------------------------------------------------------

static void buildLockPath(char* lockPath, size_t size) {
    const char* runtimeDir = getenv("XDG_RUNTIME_DIR");
    if (runtimeDir && runtimeDir[0] != '\0') {
        snprintf(lockPath, size, "%s/stay-awake.pid", runtimeDir);
        return;
    }
    const char* home = getenv("HOME");
    if (home && home[0] != '\0') {
        snprintf(lockPath, size, "%s/.config/stay-awake.pid", home);
        return;
    }
    snprintf(lockPath, size, "/tmp/stay-awake-%d.pid", (int)getuid());
}

static int acquireLock(void) {
    char lockPath[512];
    buildLockPath(lockPath, sizeof(lockPath));

    int lockFd = open(lockPath, O_CREAT | O_RDWR | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (lockFd < 0) {
        return -1;
    }
    fchmod(lockFd, 0600);

    if (flock(lockFd, LOCK_EX | LOCK_NB) != 0) {
        if (errno == EWOULDBLOCK) {
            // Another instance is running — read its PID and signal it
            char buf[32] = {0};
            if (read(lockFd, buf, sizeof(buf) - 1) > 0) {
                char* endPtr = NULL;
                long  oldPid = strtol(buf, &endPtr, 10);
                if (oldPid > 0 && endPtr != buf) {
                    kill((pid_t)oldPid, SIGTERM);
                    g_usleep((gulong)RESTART_DELAY_MS * 1000);
                }
            }
            // Try again
            if (flock(lockFd, LOCK_EX | LOCK_NB) != 0) {
                close(lockFd);
                return -1;
            }
        } else {
            close(lockFd);
            return -1;
        }
    }

    // Write our PID
    ftruncate(lockFd, 0);
    lseek(lockFd, 0, SEEK_SET);
    char pidStr[32];
    snprintf(pidStr, sizeof(pidStr), "%d", getpid());
    write(lockFd, pidStr, strlen(pidStr));

    return lockFd; // keep open to hold the lock
}

// ---------------------------------------------------------------------------
// Entry point
// ---------------------------------------------------------------------------

int main(int argc, char* argv[]) { // NOLINT(*-function-cognitive-complexity)
    // Single instance check
    int lockFd = acquireLock();
    if (lockFd < 0) {
        return 1;
    }

    initConfigPath();
    loadConfig();
    updateAutostartIfNeeded();

    // Resolve icon directory from executable path
    ssize_t exeLen = readlink("/proc/self/exe", iconDir, sizeof(iconDir) - 1);
    if (exeLen > 0) {
        iconDir[exeLen] = '\0';
        char* lastSlash = strrchr(iconDir, '/');
        if (lastSlash) {
            *lastSlash = '\0';
        }
    } else {
        snprintf(iconDir, sizeof(iconDir), ".");
    }

    // Detect dark mode and switch to light icon set if needed
    {
        GSettingsSchemaSource* source = g_settings_schema_source_get_default();
        GSettingsSchema*       schema =
            source ? g_settings_schema_source_lookup(source, "org.gnome.desktop.interface", TRUE) : NULL;
        if (schema) {
            GSettings* settings    = g_settings_new("org.gnome.desktop.interface");
            gchar*     colorScheme = g_settings_get_string(settings, "color-scheme");
            if (colorScheme && g_strcmp0(colorScheme, "prefer-dark") != 0) {
                // Light mode: use light/ subdirectory with dark outlines
                char lightDir[600];
                snprintf(lightDir, sizeof(lightDir), "%s/light", iconDir);
                snprintf(iconDir, sizeof(iconDir), "%s", lightDir);
            }
            g_free(colorScheme);
            g_object_unref(settings);
            g_settings_schema_unref(schema);
        }
    }

    gtk_init(&argc, &argv);

    // --- Tray icon with context menu ---
    GtkWidget* trayMenu = gtk_menu_new();

    GtkWidget* toggleItem =
        gtk_menu_item_new_with_label(globalMode == MODE_STAY_AWAKE ? "Switch to Auto-Off" : "Switch to StayAwake");
    toggleMenuItem = GTK_MENU_ITEM(toggleItem);
    g_signal_connect(toggleItem, "activate", G_CALLBACK(onToggleMode), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(trayMenu), toggleItem);

    GtkWidget* monitorOffItem = gtk_menu_item_new_with_label("Turn Off Monitor");
    g_signal_connect(monitorOffItem, "activate", G_CALLBACK(onMonitorOff), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(trayMenu), monitorOffItem);

    GtkWidget* separator = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(trayMenu), separator);

    GtkWidget* exitItem = gtk_menu_item_new_with_label("Exit");
    g_signal_connect(exitItem, "activate", G_CALLBACK(gtk_main_quit), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(trayMenu), exitItem);
    gtk_widget_show_all(trayMenu);

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
    indicator =
        app_indicator_new_with_path("stay-awake", "dialog-warning", APP_INDICATOR_CATEGORY_APPLICATION_STATUS, iconDir);
#pragma GCC diagnostic pop
    app_indicator_set_status(indicator, APP_INDICATOR_STATUS_ACTIVE);
    app_indicator_set_menu(APP_INDICATOR(indicator), GTK_MENU(trayMenu));
    app_indicator_set_title(indicator, "StayAwake");

    const char* iconName = (globalMode == MODE_STAY_AWAKE) ? "awake" : "auto_off_000";
    app_indicator_set_icon_full(indicator, iconName, "StayAwake");
    updateTooltip();

    // --- Config file watching (GFileMonitor) ---
    {
        GFile* configDirFile = g_file_new_for_path(configDir);
        configMonitor        = g_file_monitor_directory(configDirFile, G_FILE_MONITOR_NONE, NULL, NULL);
        g_object_unref(configDirFile);
        if (configMonitor) {
            g_signal_connect(configMonitor, "changed", G_CALLBACK(onConfigChanged), NULL);
        }
    }

    // --- Power management ---
    applyPowerState();

    // Handle SIGTERM for graceful shutdown (single-instance restart)
    signalWatchId = g_unix_signal_add(SIGTERM, onSigTerm, NULL);

    gtk_main();

    cleanup();
    close(lockFd);
    return 0;
}

#endif // __linux__
