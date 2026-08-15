#include "config.h"

#include "app_state.h"

// ===========================================================================
// Windows
// ===========================================================================
#ifdef _WIN32

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

        fwprintf(SAFE_CONFIG_FILE, L"%d %d", (int)globalMode, idleLimit);
        fclose(SAFE_CONFIG_FILE);

        if (!MoveFileW(tempPath, configPath)) {
            FILE* directFile = NULL;
            if (_wfopen_s(&directFile, configPath, L"w") == 0 && directFile) {
                fwprintf(directFile, L"%d %d", (int)globalMode, idleLimit);
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
                globalMode = (AppMode)tempMode;
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

#endif // _WIN32

// ===========================================================================
// Linux
// ===========================================================================
#ifdef __linux__

// ---------------------------------------------------------------------------
// Config path
// ---------------------------------------------------------------------------

void initConfigPath(void) {
    const char* xdgConfig = getenv("XDG_CONFIG_HOME");
    if (!xdgConfig || xdgConfig[0] == '\0') {
        const char* home = getenv("HOME");
        if (home) {
            snprintf(configDir, sizeof(configDir), "%s/.config/stay-awake", home);
        } else {
            snprintf(configDir, sizeof(configDir), "/tmp/stay-awake");
        }
    } else {
        snprintf(configDir, sizeof(configDir), "%s/stay-awake", xdgConfig);
    }

    // Create directory
    g_mkdir_with_parents(configDir, 0755);

    snprintf(configPath, sizeof(configPath), "%s/%s", configDir, CONFIG_FILENAME);
}

// ---------------------------------------------------------------------------
// Persist / restore
// ---------------------------------------------------------------------------

void saveConfig(void) {
    char tempPath[512];
    snprintf(tempPath, sizeof(tempPath), "%s.tmp", configPath);

    FILE* file = fopen(tempPath, "w");
    if (file) {
        fprintf(file, "%d %d", (int)globalMode, idleLimit);
        fclose(file);

        if (rename(tempPath, configPath) != 0) {
            // Fallback: direct write
            FILE* direct = fopen(configPath, "w");
            if (direct) {
                fprintf(direct, "%d %d", (int)globalMode, idleLimit);
                fclose(direct);
            }
            unlink(tempPath);
        }
    }
}

void loadConfig(void) {
    FILE* file = fopen(configPath, "r");
    if (file) {
        int  tempLimit = 0;
        int  tempMode  = 0;
        char line[64]  = {0};

        if (fgets(line, sizeof(line), file)) {
            if (sscanf(line, "%d %d", &tempMode, &tempLimit) == 2) {
                if (tempLimit >= 10 && tempLimit <= 86400) {
                    idleLimit = tempLimit;
                }
                if (tempMode >= 0 && tempMode < MODE_COUNT) {
                    globalMode = (AppMode)tempMode;
                }
            }
        }
        fclose(file);
    }
}

// ---------------------------------------------------------------------------
// Autostart (XDG .desktop file)
// ---------------------------------------------------------------------------

void updateAutostartIfNeeded(void) {
    const char* home = getenv("HOME");
    if (!home) {
        return;
    }

    char autostartDir[512];
    snprintf(autostartDir, sizeof(autostartDir), "%s/.config/autostart", home);

    // Ensure autostart directory exists
    g_mkdir_with_parents(autostartDir, 0755);

    char desktopPath[600];
    snprintf(desktopPath, sizeof(desktopPath), "%s/stay-awake.desktop", autostartDir);

    // Check if file already exists and matches
    FILE* existing = fopen(desktopPath, "r");
    if (existing) {
        char line[512];
        int  found = 0;
        while (fgets(line, sizeof(line), existing)) {
            if (strncmp(line, "Exec=", 5) == 0) {
                // Trim newline
                size_t len = strlen(line);
                if (len > 0 && line[len - 1] == '\n') {
                    line[len - 1] = '\0';
                }
                // Get current executable path
                char    selfPath[512];
                ssize_t selfLen = readlink("/proc/self/exe", selfPath, sizeof(selfPath) - 1);
                if (selfLen > 0) {
                    selfPath[selfLen] = '\0';
                    if (strcmp(line + 5, selfPath) == 0) {
                        found = 1;
                    }
                }
                break;
            }
        }
        fclose(existing);
        if (found) {
            return; // Already correct
        }
    }

    // Get current executable path
    char    selfPath[512];
    ssize_t selfLen = readlink("/proc/self/exe", selfPath, sizeof(selfPath) - 1);
    if (selfLen <= 0) {
        return;
    }
    selfPath[selfLen] = '\0';

    // Write .desktop file
    FILE* file = fopen(desktopPath, "w");
    if (file) {
        fprintf(file,
                "[Desktop Entry]\n"
                "Type=Application\n"
                "Name=StayAwake\n"
                "Comment=Prevent system sleep\n"
                "Exec=%s\n"
                "Icon=dialog-warning\n"
                "Terminal=false\n"
                "X-GNOME-Autostart-enabled=true\n",
                selfPath);
        fclose(file);
    }
}

#endif // __linux__
