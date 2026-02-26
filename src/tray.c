#include "tray.h"

#include <stdio.h>
#include <string.h>

// Colors used exclusively for icon rendering
static const COLORREF S_COL_RED  = RGB(231, 76, 60);
static const COLORREF S_COL_BLUE = RGB(52, 152, 219);
static const COLORREF S_COL_DARK = RGB(45, 45, 45);

// ---------------------------------------------------------------------------
// Icon drawing
// ---------------------------------------------------------------------------

HICON createDynamicIcon(const int idle, const AppMode mode) {
    const int SIZE = GetSystemMetrics(SM_CXSMICON);

    // ReSharper disable once CppLocalVariableMayBeConst
    HDC hdcScreen = GetDC(NULL);
    if (!hdcScreen) {
        return NULL;
    }

    // ReSharper disable once CppLocalVariableMayBeConst
    HDC memDC = CreateCompatibleDC(hdcScreen);
    if (!memDC) {
        ReleaseDC(NULL, hdcScreen);
        return NULL;
    }

    BITMAPINFO bmi              = {0};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = SIZE;
    bmi.bmiHeader.biHeight      = SIZE;
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = BIT_DEPTH_32;
    bmi.bmiHeader.biCompression = BI_RGB;

    void* bits = NULL;
    // ReSharper disable once CppLocalVariableMayBeConst
    HBITMAP hColorBmp = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!hColorBmp) {
        DeleteDC(memDC);
        ReleaseDC(NULL, hdcScreen);
        return NULL;
    }

    // ReSharper disable once CppLocalVariableMayBeConst
    HBITMAP hOldBmp = SelectObject(memDC, hColorBmp);
    ZeroMemory(bits, (size_t)SIZE * SIZE * 4);

    const int X_LEFT   = 1;
    const int Y_TOP    = 1;
    const int X_RIGHT  = SIZE - 2;
    const int Y_BOTTOM = SIZE - 5;
    const int CENTER_X = SIZE / 2;

    // Background fill
    // ReSharper disable once CppLocalVariableMayBeConst
    HBRUSH hFill = CreateSolidBrush(mode == MODE_STAY_AWAKE ? S_COL_RED : S_COL_DARK);
    // ReSharper disable once CppLocalVariableMayBeConst
    RECT fillRect = {X_LEFT + 1, Y_TOP + 1, X_RIGHT, Y_BOTTOM};
    FillRect(memDC, &fillRect, hFill);
    DeleteObject(hFill);

    // Progress bar (auto-off mode)
    if (mode == MODE_AUTO_OFF && idle > 0) {
        const int HEIGHT = fillRect.bottom - fillRect.top;
        int       fill   = idle * HEIGHT / idleLimit;
        if (fill > HEIGHT) {
            fill = HEIGHT;
        }
        // ReSharper disable once CppLocalVariableMayBeConst
        RECT progRect = {fillRect.left, fillRect.bottom - fill, fillRect.right, fillRect.bottom};
        // ReSharper disable once CppLocalVariableMayBeConst
        HBRUSH hBlue = CreateSolidBrush(S_COL_BLUE);
        FillRect(memDC, &progRect, hBlue);
        DeleteObject(hBlue);
    }

    // Outline + base line
    // ReSharper disable once CppLocalVariableMayBeConst
    HPEN hWhitePen = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
    // ReSharper disable once CppLocalVariableMayBeConst
    HGDIOBJ oldPen = SelectObject(memDC, hWhitePen);
    SelectObject(memDC, GetStockObject(NULL_BRUSH));
    RoundRect(memDC, X_LEFT, Y_TOP, X_RIGHT + 1, Y_BOTTOM + 1, 3, 3);
    MoveToEx(memDC, CENTER_X - 4, Y_BOTTOM + 3, NULL);
    LineTo(memDC, CENTER_X + 4, Y_BOTTOM + 3);
    SelectObject(memDC, oldPen);
    DeleteObject(hWhitePen);

    // Mask bitmap
    // ReSharper disable once CppLocalVariableMayBeConst
    HBITMAP hMaskBmp = CreateBitmap(SIZE, SIZE, 1, 1, NULL);
    if (!hMaskBmp) {
        SelectObject(memDC, hOldBmp);
        DeleteObject(hColorBmp);
        DeleteDC(memDC);
        ReleaseDC(NULL, hdcScreen);
        return NULL;
    }

    // ReSharper disable once CppLocalVariableMayBeConst
    HDC maskDC = CreateCompatibleDC(hdcScreen);
    if (!maskDC) {
        DeleteObject(hMaskBmp);
        SelectObject(memDC, hOldBmp);
        DeleteObject(hColorBmp);
        DeleteDC(memDC);
        ReleaseDC(NULL, hdcScreen);
        return NULL;
    }

    // ReSharper disable once CppLocalVariableMayBeConst
    HBITMAP hOldMask = SelectObject(maskDC, hMaskBmp);
    PatBlt(maskDC, 0, 0, SIZE, SIZE, WHITENESS);
    SelectObject(maskDC, GetStockObject(BLACK_BRUSH));
    RoundRect(maskDC, X_LEFT, Y_TOP, X_RIGHT + 1, Y_BOTTOM + 1, 3, 3);

    const RECT NECK_MASK = {CENTER_X - 1, Y_BOTTOM + 1, CENTER_X + 1, Y_BOTTOM + 2};
    FillRect(maskDC, &NECK_MASK, GetStockObject(BLACK_BRUSH));
    const RECT BASE_MASK = {CENTER_X - 4, Y_BOTTOM + 3, CENTER_X + 4, Y_BOTTOM + 4};
    FillRect(maskDC, &BASE_MASK, GetStockObject(BLACK_BRUSH));

    SelectObject(maskDC, hOldMask);

    ICONINFO iconInfo = {TRUE, 0, 0, hMaskBmp, hColorBmp};
    // ReSharper disable once CppLocalVariableMayBeConst
    HICON hIcon = CreateIconIndirect(&iconInfo);

    SelectObject(memDC, hOldBmp);
    DeleteObject(hColorBmp);
    DeleteObject(hMaskBmp);
    DeleteDC(memDC);
    DeleteDC(maskDC);
    ReleaseDC(NULL, hdcScreen);

    return hIcon;
}

// ---------------------------------------------------------------------------
// Tray helpers
// ---------------------------------------------------------------------------

void updateTray(const int idle) {
    // ReSharper disable once CppLocalVariableMayBeConst
    HICON hNew = createDynamicIcon(idle, globalMode);
    if (hNew) {
        // ReSharper disable once CppLocalVariableMayBeConst
        HICON hOld       = notifyData.hIcon;
        notifyData.hIcon = hNew;
        notifyData.uFlags |= NIF_ICON;
        Shell_NotifyIconW(NIM_MODIFY, &notifyData);
        if (hOld) {
            DestroyIcon(hOld);
        }
    }
    updateTooltip();
}

void updateTooltip(void) {
    wchar_t tooltip[128];

    if (globalMode == MODE_STAY_AWAKE) {
        swprintf_s(tooltip, 128, L"StayAwake v%d.%d.%d: Always on", APP_VERSION_MAJOR, APP_VERSION_MINOR,
                   APP_VERSION_PATCH);
    } else {
        swprintf_s(tooltip, 128, L"StayAwake v%d.%d.%d: Auto-off (%d sec)", APP_VERSION_MAJOR, APP_VERSION_MINOR,
                   APP_VERSION_PATCH, idleLimit);
    }

    wcscpy_s(notifyData.szTip, 128, tooltip);
    notifyData.uFlags |= NIF_TIP;
    Shell_NotifyIconW(NIM_MODIFY, &notifyData);
}