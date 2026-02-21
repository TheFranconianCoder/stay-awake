#include "power.h"
#include "config.h"
#include "tray.h"

// ---------------------------------------------------------------------------
// Power state
// ---------------------------------------------------------------------------

void ApplyPowerState(void) {
    SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED |
                            (g_mode == MODE_STAY_AWAKE ? ES_DISPLAY_REQUIRED : 0));

    if (g_mode == MODE_AUTO_OFF) {
        SetTimer(g_notifyData.hWnd, ID_TIMER_TICK, MS_PER_SEC, NULL);
    } else {
        KillTimer(g_notifyData.hWnd, ID_TIMER_TICK);
    }

    g_monitorIsOff = FALSE;
}

// ---------------------------------------------------------------------------
// Window procedure
// ---------------------------------------------------------------------------

// ReSharper disable once CppParameterMayBeConst
LRESULT CALLBACK WndProc(HWND hwnd, const UINT msg, const WPARAM wParam, const LPARAM lParam) { // NOLINT(*-function-cognitive-complexity)
    switch (msg) {
    case WM_TRAYICON:
        if (lParam == WM_LBUTTONUP) {
            if (GetKeyState(VK_SHIFT) & 0x8000) {
                SendMessageW(HWND_BROADCAST, WM_SYSCOMMAND, SC_MONITORPOWER, 2);
                g_monitorIsOff = TRUE;
            } else {
                g_mode = g_mode == MODE_STAY_AWAKE ? MODE_AUTO_OFF : MODE_STAY_AWAKE;
                ApplyPowerState();
                SaveConfig();
                UpdateTray(0);
                UpdateTooltip();
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
                    const int idle = (int)((currentTicks - inputInfo.dwTime) / MS_PER_SEC);
                    if (idle >= 0 && g_mode == MODE_AUTO_OFF) {
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
        break;

    case WM_DPICHANGED:
        UpdateTray(0);
        return 0;

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