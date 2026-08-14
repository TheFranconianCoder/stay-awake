#pragma once

#include "app_state.h"

#ifdef _WIN32
HICON createDynamicIcon(int idle, AppMode mode);
#endif

void updateTray(int idle);
void updateTooltip(void);
