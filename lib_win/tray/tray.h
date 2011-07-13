#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef BOOL (*minimize_callback_t)(void *handle, int reason);

typedef struct _mouseevent_t {
  int button;
  int clickCount;
  long x;
  long y;
  int keys;
} mouseevent_t;

typedef void (*mouseevent_callback_t)(void *handle, mouseevent_t *event);


void WINAPI mintrayr_Init();
void WINAPI mintrayr_Destroy();

void* WINAPI mintrayr_GetBaseWindow(wchar_t *title);

BOOL WINAPI mintrayr_WatchWindow(void *handle, minimize_callback_t callback);
BOOL WINAPI mintrayr_UnwatchWindow(void *handle);

void WINAPI mintrayr_MinimizeWindow(void *handle);
void WINAPI mintrayr_RestoreWindow(void *handle);


BOOL WINAPI mintrayr_CreateIcon(void *handle, mouseevent_callback_t callback);
BOOL WINAPI mintrayr_DestroyIcon(void *handle);

#ifdef __cplusplus
} // extern "C"
#endif