// ReSharper disable CppLocalVariableMayBeConst
// ReSharper disable CppParameterMayBeConst
// ReSharper disable CppRedundantCastExpression

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <stdio.h>

enum {
    ID_TRAY_ICON = 101,
    ID_TRAY_EXIT = 102,
    ID_TIMER_TICK = 1001,
    DEFAULT_IDLE_LIMIT = 300,
    BIT_DEPTH_32 = 32,
    MS_PER_SEC = 1000,
    RESTART_DELAY_MS = 500
};

#define WM_TRAYICON (WM_USER + 1)

typedef enum {
    MODE_STAY_AWAKE = 0,
    MODE_AUTO_OFF,
    MODE_COUNT
} AppMode;

int                   g_idleLimit = DEFAULT_IDLE_LIMIT;
static const COLORREF g_colRed    = RGB(231, 76, 60);
static const COLORREF g_colBlue   = RGB(52, 152, 219);
static const COLORREF g_colDark   = RGB(45, 45, 45);

NOTIFYICONDATAW g_notifyData   = {0};
AppMode         g_mode         = MODE_STAY_AWAKE;
BOOL            g_monitorIsOff = FALSE;
wchar_t         g_configPath[MAX_PATH];
wchar_t         g_configDir[MAX_PATH];
DWORD64         g_lastConfigLoad = 0;

void InitConfigPath() {
    PWSTR pathLocal;
    if (SHGetKnownFolderPath(&FOLDERID_LocalAppData, 0, NULL, &pathLocal) == S_OK) {
        if (wcslen(pathLocal) + 20 < MAX_PATH) {
            swprintf(g_configDir, MAX_PATH, L"%s\\StayAwake", pathLocal);
            CreateDirectoryW(g_configDir, NULL);
            swprintf(g_configPath, MAX_PATH, L"%s\\stay_awake.conf", g_configDir);
        }
        CoTaskMemFree(pathLocal);
    }
}

void SaveConfig() {
    wchar_t tempPath[MAX_PATH];
    swprintf(tempPath, MAX_PATH, L"%s.tmp", g_configPath);

    FILE *const configFile = _wfopen(tempPath, L"w");
    if (configFile) {
        fwprintf(configFile, L"%d %d", (int)g_mode, g_idleLimit);
        fclose(configFile);

        if (!MoveFileW(tempPath, g_configPath)) {
            FILE *directFile = _wfopen(g_configPath, L"w");
            if (directFile) {
                fwprintf(directFile, L"%d %d", (int)g_mode, g_idleLimit);
                fclose(directFile);
            }
            DeleteFileW(tempPath);
        }
    }
}

void LoadConfig() {
    FILE *configFile = _wfopen(g_configPath, L"r");
    if (configFile) {
        int tempLimit = 0;
        int tempMode  = 0;

        if (fwscanf(configFile, L"%d %d", &tempMode, &tempLimit) == 2) {
            if (tempLimit >= 10 && tempLimit <= 86400) g_idleLimit = tempLimit;
            if (tempMode >= 0 && tempMode < MODE_COUNT) g_mode = (AppMode)tempMode;
        }
        fclose(configFile);
    }
}

void UpdateTooltip() {
    wchar_t tooltip[128];
    if (g_mode == MODE_STAY_AWAKE) {
        wcscpy_s(tooltip, 128, L"StayAwake: Always on");
    } else {
        swprintf_s(tooltip, 128, L"StayAwake: Auto-off (%d sec)", g_idleLimit);
    }
    wcscpy_s(g_notifyData.szTip, 128, tooltip);
    g_notifyData.uFlags |= NIF_TIP;
    Shell_NotifyIconW(NIM_MODIFY, &g_notifyData);
}

HICON CreateDynamicIcon(const int idle, const AppMode mode) {
    const int size = GetSystemMetrics(SM_CXSMICON);

    HDC hdcScreen = GetDC(NULL);
    if (!hdcScreen) return NULL;

    HDC memDC = CreateCompatibleDC(hdcScreen);
    if (!memDC) {
        ReleaseDC(NULL, hdcScreen);
        return NULL;
    }

    BITMAPINFO bmi              = {0};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = size;
    bmi.bmiHeader.biHeight      = size;
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = BIT_DEPTH_32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void *  bits      = NULL;
    HBITMAP hColorBmp = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!hColorBmp) {
        DeleteDC(memDC);
        ReleaseDC(NULL, hdcScreen);
        return NULL;
    }

    HBITMAP hOldBmp = SelectObject(memDC, hColorBmp);

    memset(bits, 0x00, (size_t)size * size * 4);

    const int xLeft   = 1;
    const int yTop    = 1;
    const int xRight  = size - 2;
    const int yBottom = size - 5;
    const int centerX = size / 2;

    HBRUSH hFill    = CreateSolidBrush(mode == MODE_STAY_AWAKE ? g_colRed : g_colDark);
    RECT   fillRect = {xLeft + 1, yTop + 1, xRight, yBottom};
    FillRect(memDC, &fillRect, hFill);
    DeleteObject(hFill);

    if (mode == MODE_AUTO_OFF && idle > 0) {
        const int height = fillRect.bottom - fillRect.top;
        int       fill   = idle * height / g_idleLimit;
        if (fill > height) fill = height;
        RECT   progRect = {fillRect.left, fillRect.bottom - fill, fillRect.right, fillRect.bottom};
        HBRUSH hBlue    = CreateSolidBrush(g_colBlue);
        FillRect(memDC, &progRect, hBlue);
        DeleteObject(hBlue);
    }

    HPEN    hWhitePen = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
    HGDIOBJ oldPen    = SelectObject(memDC, hWhitePen);
    SelectObject(memDC, GetStockObject(NULL_BRUSH));
    RoundRect(memDC, xLeft, yTop, xRight + 1, yBottom + 1, 3, 3);

    MoveToEx(memDC, centerX - 4, yBottom + 3, NULL);
    LineTo(memDC, centerX + 4, yBottom + 3);

    SelectObject(memDC, oldPen);
    DeleteObject(hWhitePen);

    HBITMAP hMaskBmp = CreateBitmap(size, size, 1, 1, NULL);
    if (!hMaskBmp) {
        SelectObject(memDC, hOldBmp);
        DeleteObject(hColorBmp);
        DeleteDC(memDC);
        ReleaseDC(NULL, hdcScreen);
        return NULL;
    }

    HDC maskDC = CreateCompatibleDC(hdcScreen);
    if (!maskDC) {
        DeleteObject(hMaskBmp);
        SelectObject(memDC, hOldBmp);
        DeleteObject(hColorBmp);
        DeleteDC(memDC);
        ReleaseDC(NULL, hdcScreen);
        return NULL;
    }

    HBITMAP hOldMask = SelectObject(maskDC, hMaskBmp);

    PatBlt(maskDC, 0, 0, size, size, WHITENESS);
    SelectObject(maskDC, GetStockObject(BLACK_BRUSH));
    RoundRect(maskDC, xLeft, yTop, xRight + 1, yBottom + 1, 3, 3);

    const RECT neckMask = {centerX - 1, yBottom + 1, centerX + 1, yBottom + 2};
    FillRect(maskDC, &neckMask, GetStockObject(BLACK_BRUSH));
    const RECT baseMask = {centerX - 4, yBottom + 3, centerX + 4, yBottom + 4};
    FillRect(maskDC, &baseMask, GetStockObject(BLACK_BRUSH));

    SelectObject(maskDC, hOldMask);
    ICONINFO iconInfo = {TRUE, 0, 0, hMaskBmp, hColorBmp};
    HICON    hIcon    = CreateIconIndirect(&iconInfo);

    SelectObject(memDC, hOldBmp);
    DeleteObject(hColorBmp);
    DeleteObject(hMaskBmp);
    DeleteDC(memDC);
    DeleteDC(maskDC);
    ReleaseDC(NULL, hdcScreen);

    return hIcon;
}

void UpdateTray(const int idle) {
    HICON hNew = CreateDynamicIcon(idle, g_mode);
    if (hNew) {
        HICON hOld          = g_notifyData.hIcon;
        g_notifyData.hIcon  = hNew;
        g_notifyData.uFlags |= NIF_ICON;
        Shell_NotifyIconW(NIM_MODIFY, &g_notifyData);
        if (hOld) DestroyIcon(hOld);
    }
    UpdateTooltip();
}

void ApplyPowerState() {
    SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED |
                            (g_mode == MODE_STAY_AWAKE ? ES_DISPLAY_REQUIRED : 0));

    if (g_mode == MODE_AUTO_OFF) {
        SetTimer(g_notifyData.hWnd, ID_TIMER_TICK, MS_PER_SEC, NULL);
    } else {
        KillTimer(g_notifyData.hWnd, ID_TIMER_TICK);
        g_monitorIsOff = FALSE;
    }
}

LRESULT CALLBACK WndProc(HWND hwnd, const UINT msg, const WPARAM wParam, const LPARAM lParam) { // NOLINT(*-function-cognitive-complexity)
    switch (msg) {
    case WM_TRAYICON:
        if (lParam == WM_LBUTTONUP) {
            g_mode = g_mode == MODE_STAY_AWAKE ? MODE_AUTO_OFF : MODE_STAY_AWAKE;
            ApplyPowerState();
            SaveConfig();
            UpdateTray(0);
            UpdateTooltip();
        } else if (lParam == WM_RBUTTONUP) {
            POINT mousePt;
            GetCursorPos(&mousePt);
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
                DWORD64 currentTicks = GetTickCount64();
                if (currentTicks >= inputInfo.dwTime) {
                    const int idle = (int)((currentTicks - inputInfo.dwTime) / MS_PER_SEC);
                    if (idle >= 0) {
                        if (g_mode == MODE_AUTO_OFF) {
                            UpdateTray(idle);
                            if (idle >= g_idleLimit && !g_monitorIsOff) {
                                SendMessageW(HWND_BROADCAST, WM_SYSCOMMAND, SC_MONITORPOWER, 2);
                                g_monitorIsOff = TRUE;
                            } else if (idle < g_idleLimit) {
                                g_monitorIsOff = FALSE;
                            }
                        }
                    }
                }
            }
        }
        break;

    case WM_DPICHANGED: {
        UpdateTray(0);
        return 0;
    }

    case WM_DESTROY:
        KillTimer(hwnd, ID_TIMER_TICK);
        SetThreadExecutionState(ES_CONTINUOUS);
        Shell_NotifyIconW(NIM_DELETE, &g_notifyData);
        if (g_notifyData.hIcon) {
            DestroyIcon(g_notifyData.hIcon);
            g_notifyData.hIcon = NULL;
        }
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}

void UpdateAutostartIfNeeded() {
    WCHAR currentPath[MAX_PATH];
    GetModuleFileNameW(NULL, currentPath, MAX_PATH);

    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ | KEY_WRITE,
                      &hKey) == ERROR_SUCCESS) {
        WCHAR   existingPath[MAX_PATH];
        DWORD   dataSize = sizeof(existingPath);
        LSTATUS status   = RegQueryValueExW(hKey, L"StayAwake", NULL, NULL, (LPBYTE)existingPath, &dataSize);

        if (status != ERROR_SUCCESS || wcscmp(existingPath, currentPath) != 0) {
            RegSetValueExW(hKey, L"StayAwake", 0, REG_SZ, (BYTE *)currentPath,
                           (wcslen(currentPath) + 1) * sizeof(WCHAR));
        }
        RegCloseKey(hKey);
    }
}

void EnableDpiAwareness() {
    HMODULE hUser32 = GetModuleHandleW(L"user32.dll");
    if (hUser32) {
        typedef BOOL (WINAPI *            SetProcessDpiAwarenessContextProc)(DPI_AWARENESS_CONTEXT);
        SetProcessDpiAwarenessContextProc setDpiContext =
            (SetProcessDpiAwarenessContextProc)GetProcAddress(hUser32, "SetProcessDpiAwarenessContext");

        if (setDpiContext) {
            setDpiContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
            return;
        }
    }
    SetProcessDPIAware();
}

int main() { // NOLINT(*-function-cognitive-complexity)
    EnableDpiAwareness();

    HANDLE hMutex = CreateMutexW(NULL, TRUE, L"StayAwake_SingleInstance_Mutex");

    if (!hMutex) {
        return 1;
    }

    if (GetLastError() == ERROR_ALREADY_EXISTS) {
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

    const WNDCLASSEXW winClass = {sizeof(WNDCLASSEXW), 0, WndProc, 0, 0, GetModuleHandle(NULL),
                                  NULL, NULL, NULL, NULL, L"SA_CLASS", NULL};
    RegisterClassExW(&winClass);
    HWND hwnd = CreateWindowExW(0, L"SA_CLASS", NULL, 0, 0, 0, 0, 0, HWND_MESSAGE, NULL, NULL, NULL);

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
    if (hNotify == INVALID_HANDLE_VALUE) {
        hNotify = NULL;
    }

    MSG msg;

    while (TRUE) {
        HANDLE waitHandles[2];
        DWORD handleCount = 0;
    
        if (hNotify) {
            waitHandles[handleCount++] = hNotify;
        }
    
        DWORD dwWait = MsgWaitForMultipleObjects(handleCount, waitHandles, FALSE, INFINITE, QS_ALLINPUT);
    
        if (dwWait == WAIT_OBJECT_0 && handleCount > 0) {
            DWORD64 now = GetTickCount64();
            if (now - g_lastConfigLoad > 100) {
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