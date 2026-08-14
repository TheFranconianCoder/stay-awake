#pragma once

#include "app_state.h"

void applyPowerState(void);

#ifdef _WIN32
LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

#ifdef __linux__
void stopIdlePolling(void);
void startIdlePolling(void);
void stopInhibit(void);
void turnOffMonitor(void);
#endif
