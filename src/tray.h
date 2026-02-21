#pragma once

#include "app_state.h"

HICON CreateDynamicIcon(int idle, AppMode mode);
void  UpdateTray(int idle);
void  UpdateTooltip(void);