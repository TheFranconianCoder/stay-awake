#include "config.h"
#include "app_state.h"

#include <shlobj.h>
#include <stdio.h>

// ---------------------------------------------------------------------------
// Config path
// ---------------------------------------------------------------------------

void InitConfigPath(void) {
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

// ---------------------------------------------------------------------------
// Persist / restore
// ---------------------------------------------------------------------------

void SaveConfig(void) {
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

void LoadConfig(void) {
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

// ---------------------------------------------------------------------------
// Autostart
// ---------------------------------------------------------------------------

void UpdateAutostartIfNeeded(void) {
    WCHAR currentPath[MAX_PATH];
    GetModuleFileNameW(NULL, currentPath, MAX_PATH);

    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER,
                      L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                      0, KEY_READ | KEY_WRITE, &hKey) == ERROR_SUCCESS) {
        WCHAR   existingPath[MAX_PATH];
        DWORD   dataSize = sizeof(existingPath);
        // ReSharper disable once CppLocalVariableMayBeConst
        LSTATUS status   = RegQueryValueExW(hKey, L"StayAwake", NULL, NULL,
                                            // ReSharper disable once CppRedundantCastExpression
                                            (LPBYTE)existingPath, &dataSize);

        if (status != ERROR_SUCCESS || wcscmp(existingPath, currentPath) != 0) {
            // ReSharper disable once CppRedundantCastExpression
            RegSetValueExW(hKey, L"StayAwake", 0, REG_SZ, (BYTE *)currentPath,
                           (wcslen(currentPath) + 1) * sizeof(WCHAR));
        }
        RegCloseKey(hKey);
    }
}