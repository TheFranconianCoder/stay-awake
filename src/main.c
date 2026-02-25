#include "app_state.h"
#include "config.h"
#include "power.h"
#include "tray.h"

// ---------------------------------------------------------------------------
// Global definitions  (declared extern in app_state.h)
// ---------------------------------------------------------------------------

int             idleLimit    = DEFAULT_IDLE_LIMIT;
AppMode         mode         = MODE_STAY_AWAKE;
BOOL            monitorIsOff = FALSE;
NOTIFYICONDATAW notifyData   = {0};
wchar_t         configPath[MAX_PATH];
wchar_t         configDir[MAX_PATH];
DWORD64         lastConfigLoad = 0;

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
    notifyData.hIcon            = createDynamicIcon(0, mode);
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