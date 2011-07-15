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
typedef void (*minimize_callback_t)(void *handle);


void WINAPI mintrayr_Init();
void WINAPI mintrayr_Destroy();

BOOL WINAPI mintrayr_WatchWindow(void *handle);
BOOL WINAPI mintrayr_UnwatchWindow(void *handle);

void WINAPI mintrayr_MinimizeWindow(void *handle);
void WINAPI mintrayr_RestoreWindow(void *handle);

BOOL WINAPI mintrayr_CreateIcon(void *handle, mouseevent_callback_t callback);
BOOL WINAPI mintrayr_DestroyIcon(void *handle);

void* WINAPI mintrayr_GetBaseWindow(wchar_t *title);
void WINAPI mintrayr_SetWatchMode(int mode);

#ifdef __cplusplus
} // extern "C"
#endif