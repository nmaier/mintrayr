/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/ */

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _mouseevent_t {
  int button;
  int clickCount;
  long x;
  long y;
  int keys;
} mouseevent_t;

typedef void (*mouseevent_callback_t)(void *handle, mouseevent_t *event);
typedef void (*minimize_callback_t)(void *handle, int type);


void mintrayr_Init();
void mintrayr_Destroy();

BOOL mintrayr_WatchWindow(void *handle, minimize_callback_t *);
BOOL mintrayr_UnwatchWindow(void *handle);

void mintrayr_MinimizeWindow(void *handle);
void mintrayr_RestoreWindow(void *handle);

BOOL mintrayr_CreateIcon(void *handle, mouseevent_callback_t callback);
BOOL mintrayr_DestroyIcon(void *handle);

void* mintrayr_GetBaseWindow(wchar_t *title);
void mintrayr_SetWatchMode(int mode);

#ifdef __cplusplus
} // extern "C"
#endif
