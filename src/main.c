#include "app_state.h"
#include "config.h"
#include "tray.h"
#include "power.h"

// ---------------------------------------------------------------------------
// Global definitions  (declared extern in app_state.h)
// ---------------------------------------------------------------------------

int             g_idleLimit      = DEFAULT_IDLE_LIMIT;
AppMode         g_mode           = MODE_STAY_AWAKE;
BOOL            g_monitorIsOff   = FALSE;
NOTIFYICONDATAW g_notifyData     = {0};
wchar_t         g_configPath[MAX_PATH];
wchar_t         g_configDir[MAX_PATH];
DWORD64         g_lastConfigLoad = 0;

// ---------------------------------------------------------------------------
// DPI awareness
// ---------------------------------------------------------------------------

static void EnableDpiAwareness(void) {
    const HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
    if (hUser32) {
        typedef BOOL (WINAPI *SetProcessDpiAwarenessContextProc)(DPI_AWARENESS_CONTEXT);
        // ReSharper disable once CppLocalVariableMayBeConst
        SetProcessDpiAwarenessContextProc setDpiContext =
            (SetProcessDpiAwarenessContextProc)GetProcAddress(hUser32, "SetProcessDpiAwarenessContext");
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
    EnableDpiAwareness();

    HANDLE hMutex = CreateMutexW(NULL, TRUE, L"StayAwake_SingleInstance_Mutex");
    if (!hMutex) return 1;

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
            if (hMutex) CloseHandle(hMutex);
            return 1;
        }
    }

    InitConfigPath();
    LoadConfig();
    UpdateAutostartIfNeeded();

    const WNDCLASSEXW winClass = {
        sizeof(WNDCLASSEXW), 0, WndProc, 0, 0,
        GetModuleHandle(NULL), NULL, NULL, NULL, NULL, L"SA_CLASS", NULL
    };
    RegisterClassExW(&winClass);

    // ReSharper disable once CppLocalVariableMayBeConst
    HWND hwnd = CreateWindowExW(0, L"SA_CLASS", NULL, 0, 0, 0, 0, 0,
                                HWND_MESSAGE, NULL, NULL, NULL);

    g_notifyData.cbSize           = sizeof(NOTIFYICONDATAW);
    g_notifyData.hWnd             = hwnd;
    g_notifyData.uID              = ID_TRAY_ICON;
    g_notifyData.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g_notifyData.uCallbackMessage = WM_TRAYICON;
    g_notifyData.hIcon            = CreateDynamicIcon(0, g_mode);
    wcscpy_s(g_notifyData.szTip, 128, L"StayAwake");

    if (!Shell_NotifyIconW(NIM_ADD, &g_notifyData)) {
        if (g_notifyData.hIcon) DestroyIcon(g_notifyData.hIcon);
        CloseHandle(hMutex);
        return 1;
    }

    ApplyPowerState();
    UpdateTooltip();

    HANDLE hNotify = FindFirstChangeNotificationW(g_configDir, FALSE, FILE_NOTIFY_CHANGE_LAST_WRITE);
    if (hNotify == INVALID_HANDLE_VALUE) hNotify = NULL;

    MSG msg;

    while (TRUE) {
        HANDLE waitHandles[2];
        DWORD  handleCount = 0;
        if (hNotify) waitHandles[handleCount++] = hNotify;

        // ReSharper disable once CppLocalVariableMayBeConst
        DWORD dwWait = MsgWaitForMultipleObjects(handleCount, waitHandles, FALSE, INFINITE, QS_ALLINPUT);

        if (dwWait == WAIT_OBJECT_0 && handleCount > 0) {
            // ReSharper disable once CppLocalVariableMayBeConst
            DWORD64 now = GetTickCount64();
            if (now - g_lastConfigLoad > CONFIG_RELOAD_DEBOUNCE_MS) {
                LoadConfig();
                ApplyPowerState();
                UpdateTray(0);
                g_lastConfigLoad = now;
            }
            FindNextChangeNotification(hNotify);
        }

        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) goto cleanup;
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

cleanup:
    if (hNotify) FindCloseChangeNotification(hNotify);
    CloseHandle(hMutex);
    return 0;
}