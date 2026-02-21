#include <stdio.h>
#include "tray.h"

// Colors used exclusively for icon rendering
static const COLORREF s_colRed  = RGB(231, 76,  60);
static const COLORREF s_colBlue = RGB( 52, 152, 219);
static const COLORREF s_colDark = RGB( 45,  45,  45);

// ---------------------------------------------------------------------------
// Icon drawing
// ---------------------------------------------------------------------------

HICON CreateDynamicIcon(const int idle, const AppMode mode) {
    const int size = GetSystemMetrics(SM_CXSMICON);

    // ReSharper disable once CppLocalVariableMayBeConst
    HDC hdcScreen = GetDC(NULL);
    if (!hdcScreen) return NULL;

    // ReSharper disable once CppLocalVariableMayBeConst
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
    // ReSharper disable once CppLocalVariableMayBeConst
    HBITMAP hColorBmp = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
    if (!hColorBmp) {
        DeleteDC(memDC);
        ReleaseDC(NULL, hdcScreen);
        return NULL;
    }

    // ReSharper disable once CppLocalVariableMayBeConst
    HBITMAP hOldBmp = SelectObject(memDC, hColorBmp);
    memset(bits, 0x00, (size_t)size * size * 4);

    const int xLeft   = 1;
    const int yTop    = 1;
    const int xRight  = size - 2;
    const int yBottom = size - 5;
    const int centerX = size / 2;

    // Background fill
    // ReSharper disable once CppLocalVariableMayBeConst
    HBRUSH hFill    = CreateSolidBrush(mode == MODE_STAY_AWAKE ? s_colRed : s_colDark);
    // ReSharper disable once CppLocalVariableMayBeConst
    RECT   fillRect = {xLeft + 1, yTop + 1, xRight, yBottom};
    FillRect(memDC, &fillRect, hFill);
    DeleteObject(hFill);

    // Progress bar (auto-off mode)
    if (mode == MODE_AUTO_OFF && idle > 0) {
        const int height = fillRect.bottom - fillRect.top;
        int       fill   = idle * height / g_idleLimit;
        if (fill > height) fill = height;
        // ReSharper disable once CppLocalVariableMayBeConst
        RECT   progRect = {fillRect.left, fillRect.bottom - fill, fillRect.right, fillRect.bottom};
        // ReSharper disable once CppLocalVariableMayBeConst
        HBRUSH hBlue    = CreateSolidBrush(s_colBlue);
        FillRect(memDC, &progRect, hBlue);
        DeleteObject(hBlue);
    }

    // Outline + base line
    // ReSharper disable once CppLocalVariableMayBeConst
    HPEN    hWhitePen = CreatePen(PS_SOLID, 1, RGB(255, 255, 255));
    // ReSharper disable once CppLocalVariableMayBeConst
    HGDIOBJ oldPen    = SelectObject(memDC, hWhitePen);
    SelectObject(memDC, GetStockObject(NULL_BRUSH));
    RoundRect(memDC, xLeft, yTop, xRight + 1, yBottom + 1, 3, 3);
    MoveToEx(memDC, centerX - 4, yBottom + 3, NULL);
    LineTo(memDC, centerX + 4, yBottom + 3);
    SelectObject(memDC, oldPen);
    DeleteObject(hWhitePen);

    // Mask bitmap
    // ReSharper disable once CppLocalVariableMayBeConst
    HBITMAP hMaskBmp = CreateBitmap(size, size, 1, 1, NULL);
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
    PatBlt(maskDC, 0, 0, size, size, WHITENESS);
    SelectObject(maskDC, GetStockObject(BLACK_BRUSH));
    RoundRect(maskDC, xLeft, yTop, xRight + 1, yBottom + 1, 3, 3);

    const RECT neckMask = {centerX - 1, yBottom + 1, centerX + 1, yBottom + 2};
    FillRect(maskDC, &neckMask, GetStockObject(BLACK_BRUSH));
    const RECT baseMask = {centerX - 4, yBottom + 3, centerX + 4, yBottom + 4};
    FillRect(maskDC, &baseMask, GetStockObject(BLACK_BRUSH));

    SelectObject(maskDC, hOldMask);

    ICONINFO iconInfo = {TRUE, 0, 0, hMaskBmp, hColorBmp};
    // ReSharper disable once CppLocalVariableMayBeConst
    HICON    hIcon    = CreateIconIndirect(&iconInfo);

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

void UpdateTray(const int idle) {
    // ReSharper disable once CppLocalVariableMayBeConst
    HICON hNew = CreateDynamicIcon(idle, g_mode);
    if (hNew) {
        // ReSharper disable once CppLocalVariableMayBeConst
        HICON hOld         = g_notifyData.hIcon;
        g_notifyData.hIcon = hNew;
        g_notifyData.uFlags |= NIF_ICON;
        Shell_NotifyIconW(NIM_MODIFY, &g_notifyData);
        if (hOld) DestroyIcon(hOld);
    }
    UpdateTooltip();
}

void UpdateTooltip(void) {
    wchar_t tooltip[128];

    if (g_mode == MODE_STAY_AWAKE) {
        swprintf_s(tooltip, 128, L"StayAwake v%d.%d.%d: Always on",
                   APP_VERSION_MAJOR, APP_VERSION_MINOR, APP_VERSION_PATCH);
    } else {
        swprintf_s(tooltip, 128, L"StayAwake v%d.%d.%d: Auto-off (%d sec)",
                   APP_VERSION_MAJOR, APP_VERSION_MINOR, APP_VERSION_PATCH, g_idleLimit);
    }

    wcscpy_s(g_notifyData.szTip, 128, tooltip);
    g_notifyData.uFlags |= NIF_TIP;
    Shell_NotifyIconW(NIM_MODIFY, &g_notifyData);
}