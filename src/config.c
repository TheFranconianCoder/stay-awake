#include "config.h"
#include "app_state.h"

#include <shlobj.h>
#include <stdio.h>

// ---------------------------------------------------------------------------
// Config path
// ---------------------------------------------------------------------------

void initConfigPath(void) {
    PWSTR pathLocal;
    if (SHGetKnownFolderPath(&FOLDERID_LocalAppData, 0, NULL, &pathLocal) == S_OK) {
        if (wcslen(pathLocal) + 20 < MAX_PATH) {
            swprintf_s(configDir, MAX_PATH, L"%s\\StayAwake", pathLocal);
            CreateDirectoryW(configDir, NULL);
            swprintf_s(configPath, MAX_PATH, L"%s\\stay_awake.conf", configDir);
        }
        CoTaskMemFree(pathLocal);
    }
}

// ---------------------------------------------------------------------------
// Persist / restore
// ---------------------------------------------------------------------------

void saveConfig(void) {
    wchar_t tempPath[MAX_PATH];
    swprintf_s(tempPath, MAX_PATH, L"%s.tmp", configPath);

    FILE* configFile = NULL;
    if (_wfopen_s(&configFile, tempPath, L"w") == 0 && configFile) {
        FILE* const SAFE_CONFIG_FILE = configFile;

        fwprintf(SAFE_CONFIG_FILE, L"%d %d", (int)mode, idleLimit);
        fclose(SAFE_CONFIG_FILE);

        if (!MoveFileW(tempPath, configPath)) {
            FILE* directFile = NULL;
            if (_wfopen_s(&directFile, configPath, L"w") == 0 && directFile) {
                fwprintf(directFile, L"%d %d", (int)mode, idleLimit);
                fclose(directFile);
            }
            DeleteFileW(tempPath);
        }
    }
}

void loadConfig(void) {
    FILE* configFile = NULL;
    if (_wfopen_s(&configFile, configPath, L"r") == 0 && configFile) {
        int tempLimit = 0;
        int tempMode  = 0;

        if (fwscanf_s(configFile, L"%d %d", &tempMode, &tempLimit) == 2) {
            if (tempLimit >= 10 && tempLimit <= 86400) {
                idleLimit = tempLimit;
            }
            if (tempMode >= 0 && tempMode < MODE_COUNT) {
                mode = (AppMode)tempMode;
            }
        }
        fclose(configFile);
    }
}

// ---------------------------------------------------------------------------
// Autostart
// ---------------------------------------------------------------------------

void updateAutostartIfNeeded(void) {
    WCHAR currentPath[MAX_PATH];
    GetModuleFileNameW(NULL, currentPath, MAX_PATH);

    HKEY hKey;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_READ | KEY_WRITE,
                      &hKey) == ERROR_SUCCESS) {
        WCHAR existingPath[MAX_PATH];
        DWORD dataSize = sizeof(existingPath);
        // ReSharper disable once CppLocalVariableMayBeConst
        LSTATUS status = RegQueryValueExW(hKey, L"StayAwake", NULL, NULL,
                                          // ReSharper disable once CppRedundantCastExpression
                                          (LPBYTE)existingPath, &dataSize);

        if (status != ERROR_SUCCESS || wcscmp(existingPath, currentPath) != 0) {
            // ReSharper disable once CppRedundantCastExpression
            RegSetValueExW(hKey, L"StayAwake", 0, REG_SZ, (BYTE*)currentPath,
                           (wcslen(currentPath) + 1) * sizeof(WCHAR));
        }
        RegCloseKey(hKey);
    }
}