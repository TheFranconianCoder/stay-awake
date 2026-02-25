#pragma once

#include "app_state.h"

HICON createDynamicIcon(int idle, AppMode mode);
void  updateTray(int idle);
void  updateTooltip(void);