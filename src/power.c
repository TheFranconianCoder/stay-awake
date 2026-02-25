#include "power.h"
#include "config.h"
#include "tray.h"

// ---------------------------------------------------------------------------
// Power state
// ---------------------------------------------------------------------------

void applyPowerState(void) {
    SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED | (mode == MODE_STAY_AWAKE ? ES_DISPLAY_REQUIRED : 0));

    if (mode == MODE_AUTO_OFF) {
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
                mode = mode == MODE_STAY_AWAKE ? MODE_AUTO_OFF : MODE_STAY_AWAKE;
                applyPowerState();
                saveConfig();
                updateTray(0);
                updateTooltip();
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
                    if (IDLE >= 0 && mode == MODE_AUTO_OFF) {
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