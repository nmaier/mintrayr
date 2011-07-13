#include <windows.h>
#include "tray.h"

static const wchar_t kTrayMessage[]  = L"_MINTRAYR_TrayMessageW";
static const wchar_t kOldProc[] = L"_MINTRAYR_WRAPPER_OLD_PROC";

static const wchar_t kWatch[] = L"_MINTRAYR_WATCH";
static const wchar_t kWatchMinimizeProc[] = L"_MINTRAYR_WATCH_MINIMIZEPROC";

static const wchar_t kIcon[] = L"_MINTRAYR_ICON";
static const wchar_t kIconData[] = L"_MINTRAYR_ICON_DATA";
static const wchar_t kIconMouseEventProc[] = L"_MINTRAYR_ICON_MOUSEEVENTPROC";

typedef BOOL (WINAPI *pChangeWindowMessageFilter)(UINT message, DWORD dwFlag);
#ifndef MGSFLT_ADD
  // Not a Vista SDK
  #	define MSGFLT_ADD 1
  #	define MSGFLT_REMOVE 2
#endif

static UINT WM_TASKBARCREATED = 0;
static UINT WM_TRAYMESSAGE = 0;

/**
 * Minimize on what actions
 */
typedef enum _eMinimizeActions {
  kTrayOnMinimize = (1 << 0),
  kTrayOnClose = (1 << 1)
} eMinimizeActions;


/**
 * Helper function that will allow us to receive some broadcast messages on Vista
 * (We need to bypass that filter if we run as Administrator, but the orginating process
 * has less priviledges)
 */
static void AdjustMessageFilters(UINT filter)
{
  HMODULE user32 = LoadLibraryW(L"user32.dll");
  if (user32 != 0) {
    pChangeWindowMessageFilter changeWindowMessageFilter =
      reinterpret_cast<pChangeWindowMessageFilter>(GetProcAddress(
        user32,
        "ChangeWindowMessageFilter"
      ));
    if (changeWindowMessageFilter != 0) {
      changeWindowMessageFilter(WM_TASKBARCREATED, filter);
    }
    FreeLibrary(user32);
  }
}

/**
 * Helper class to get Windows Version information
 */
class OSVersionInfo : public OSVERSIONINFOEXW
{
public:
  OSVersionInfo() {
    dwOSVersionInfoSize = sizeof(OSVERSIONINFOEXW);
    ::GetVersionExW(reinterpret_cast<LPOSVERSIONINFOW>(this));
  }
  bool isVistaOrLater() {
    return dwMajorVersion >= 6;
  }
};

/**
 * Helper: Subclassed Windows WNDPROC
 */
static LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  if (::GetPropW(hwnd, kWatch) > (HANDLE)0x0) {
    // Watcher stuff

    switch (uMsg) {
    case WM_WINDOWPOSCHANGING: {
      /* XXX Fix this bit to something more reasonable
         The following code kinda replicates the way mozilla gets the window state.
         We intensionally "hide" the SW_SHOWMINIMIZED here.
         This indeed might cause some side effects, but if it didn't we couldn't open
         menus due to bugzilla #435848.
         This might defeat said bugfix completely reverting to old behavior, but only when we're active, of course.
         */
      WINDOWPOS *wp = reinterpret_cast<WINDOWPOS*>(lParam);
      if (wp == 0) {
        goto WndProcEnd;
      }
      if (wp->flags & SWP_FRAMECHANGED && ::IsWindowVisible(hwnd)) {
        WINDOWPLACEMENT pl;
        pl.length = sizeof(WINDOWPLACEMENT);
        ::GetWindowPlacement(hwnd, &pl);
        if (pl.showCmd == SW_SHOWMINIMIZED) {
          return 0;
        }
      }
      break;
    }
    case WM_WINDOWPOSCHANGED: {
      /* XXX Fix this bit to something more reasonable
         The following code kinda replicates the way mozilla gets the window state.
         We intensionally "hide" the SW_SHOWMINIMIZED here.
         This indeed might cause some side effects, but if it didn't we couldn't open
         menus due to bugzilla #435848,.
         This might defeat said bugfix completely reverting to old behavior, but only when we're active, of course.
      */
      WINDOWPOS *wp = reinterpret_cast<WINDOWPOS*>(lParam);
      if (wp == 0) {
        goto WndProcEnd;
      }
      /*if (wp->flags & SWP_SHOWWINDOW) {
        // Shown again, unexpectedly that is, so release
        Icon *me = reinterpret_cast<Icon*>(GetPropW(hwnd, kPlatformIcon));
        if (me == 0 || me->mOwnerIcon == 0 || me->mOwnerIcon->IsClosed()) {
          goto WndProcEnd;
        }
        me->mOwnerIcon->Restore();
      }
      else*/ if (wp->flags & SWP_FRAMECHANGED && ::IsWindowVisible(hwnd)) {
        WINDOWPLACEMENT pl;
        pl.length = sizeof(WINDOWPLACEMENT);
        ::GetWindowPlacement(hwnd, &pl);
        if (pl.showCmd == SW_SHOWMINIMIZED) {
          minimize_callback_t callback = reinterpret_cast<minimize_callback_t>(GetPropW(hwnd, kWatchMinimizeProc));
          if (callback && callback(hwnd, kTrayOnMinimize)) {
            // We're active, ignore
            return 0;
          }
        }
      }
      break;
    } // case WM_WINDOWPOSCHANGED
    case WM_NCLBUTTONDOWN:
    case WM_NCLBUTTONUP:
      // Frame button clicked
      if (wParam == HTCLOSE) {
        minimize_callback_t callback = reinterpret_cast<minimize_callback_t>(GetPropW(hwnd, kWatchMinimizeProc));
        if (callback && callback(hwnd, kTrayOnMinimize)) {
          return TRUE;
        }
      }
      break;

    case WM_SYSCOMMAND:
      // Window menu
      if (wParam == SC_CLOSE) {
        minimize_callback_t callback = reinterpret_cast<minimize_callback_t>(GetPropW(hwnd, kWatchMinimizeProc));
        if (callback && callback(hwnd, kTrayOnMinimize)) {
          return 0;
        }
      }
      break;
    }
  }

  if (::GetPropW(hwnd, kIcon) == (HANDLE)0x1) {
    // Icon stuff

    // This is a badly documented custom broadcast message by explorer
    if (uMsg == WM_TASKBARCREATED) {
      // Try to get the platform icon
      NOTIFYICONDATAW *iconData = reinterpret_cast<NOTIFYICONDATAW*>(GetPropW(hwnd, kIconData));
      if (iconData == 0) {
        goto WndProcEnd;
      }
      // The taskbar was (re)created. Add ourselves again.
      Shell_NotifyIconW(NIM_ADD, iconData);
    }

    // We got clicked. How exciting, isn't it.
    else if (uMsg == WM_TRAYMESSAGE) {
      mouseevent_t event;
      event.clickCount = 0;
      switch (LOWORD(lParam)) {
        case WM_LBUTTONUP:
        case WM_MBUTTONUP:
        case WM_RBUTTONUP:
        case WM_CONTEXTMENU:
        case NIN_KEYSELECT:
          event.clickCount = 1;
          break;
        case WM_LBUTTONDBLCLK:
        case WM_MBUTTONDBLCLK:
        case WM_RBUTTONDBLCLK:
          event.clickCount = 2;
          break;
      }
      switch (LOWORD(lParam)) {
        case WM_LBUTTONUP:
        case WM_LBUTTONDBLCLK:
          event.button = 0;
          break;
        case WM_MBUTTONUP:
        case WM_MBUTTONDBLCLK:
          event.button = 1;
          break;
        case WM_RBUTTONUP:
        case WM_RBUTTONDBLCLK:
        case WM_CONTEXTMENU:
        case NIN_KEYSELECT:
          event.button = 2;
          break;
      }
      if (event.clickCount) {
        POINT wpt;
        if (GetCursorPos(&wpt) == TRUE) {
          event.x = wpt.x;
          event.y = wpt.y;

          event.keys = 0;
          if (::GetKeyState(VK_CONTROL) & 0x8000) {
            event.keys += (1<<0);
          }
          if (::GetKeyState(VK_MENU) & 0x8000) {
            event.keys += (1<<1);
          }
          if (::GetKeyState(VK_MENU) & 0x8000) {
            event.keys += (1<<2);
          }

          // SFW/PM is a win32 hack, so that the context menu is hidden when loosing focus.
          ::SetForegroundWindow(hwnd);

          // Try to get the platform icon
          mouseevent_callback_t callback = reinterpret_cast<mouseevent_callback_t>(GetPropW(hwnd, kIconMouseEventProc));
          if (callback) {
            callback(hwnd, &event);
          }

          ::PostMessage(hwnd, WM_NULL, 0, 0L);
        }
      }
      return 0;
    }

    // Window title changed
    else if (uMsg == WM_SETTEXT) {
      NOTIFYICONDATAW *iconData = reinterpret_cast<NOTIFYICONDATAW*>(GetPropW(hwnd, kIconData));
      if (iconData == 0) {
        goto WndProcEnd;
      }

      // First, let the original wndproc process this message,
      // so that we may query the thing afterwards ;)
      // this is required because we cannot know the encoding of this message for sure ;)
      LRESULT rv;
      WNDPROC oldWindowProc = reinterpret_cast<WNDPROC>(::GetPropW(hwnd, kOldProc));
      if (oldWindowProc != 0) {
        rv = CallWindowProcW(oldWindowProc, hwnd, uMsg, wParam, lParam);
      }
      else {
        rv = DefWindowProcW(hwnd, uMsg, wParam, lParam);
      }

      if (::GetWindowTextW(hwnd, iconData->szTip, 127) != 0) {
        iconData->szTip[128] = '\0';
        Shell_NotifyIconW(NIM_MODIFY, iconData);
      }
      return rv;
    }
  }

WndProcEnd:
  // Call the old WNDPROC or at lest DefWindowProc
  WNDPROC oldProc = reinterpret_cast<WNDPROC>(::GetPropW(hwnd, kOldProc));
  if (oldProc != 0) {
    return ::CallWindowProcW(oldProc, hwnd, uMsg, wParam, lParam);
  }
  return ::DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

void WINAPI mintrayr_Init()
{
  // Get TaskbarCreated
  WM_TASKBARCREATED = RegisterWindowMessageW(L"TaskbarCreated");
  // We register this as well, as we cannot know which WM_USER values are already taken
  WM_TRAYMESSAGE = RegisterWindowMessageW(kTrayMessage);

  // Vista (Administrator) needs some love, or else we won't receive anything due to UIPI
  if (OSVersionInfo().isVistaOrLater()) {
    AdjustMessageFilters(MSGFLT_ADD);
  }
}

void WINAPI mintrayr_Destroy()
{
  // Vista (Administrator) needs some unlove, see c'tor
  if (OSVersionInfo().isVistaOrLater()) {
    AdjustMessageFilters(MSGFLT_REMOVE);
  }
}

static void SetupWnd(HWND hwnd)
{
  if (::GetPropW(hwnd, kOldProc) == 0) {
    WNDPROC oldProc = reinterpret_cast<WNDPROC>(::SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WndProc)));
    ::SetPropW(hwnd, kOldProc, reinterpret_cast<HANDLE>(oldProc));
  }
}

BOOL WINAPI mintrayr_WatchWindow(void *handle, minimize_callback_t callback)
{
  HWND hwnd = (HWND)handle;
  if (!hwnd) {
    return FALSE;
  }

  SetupWnd(hwnd);
  ::SetPropW(hwnd, kWatchMinimizeProc, reinterpret_cast<HANDLE>(callback));
  ::SetPropW(hwnd, kWatch, reinterpret_cast<HANDLE>(0x1));

  return TRUE;
}
BOOL WINAPI mintrayr_UnwatchWindow(void *handle)
{
  HWND hwnd = (HWND)handle;
  if (!hwnd) {
    return FALSE;
  }

  ::RemovePropW(hwnd, kWatch);
  ::RemovePropW(hwnd, kWatchMinimizeProc);

  return TRUE;
}

void WINAPI mintrayr_MinimizeWindow(void *handle)
{
  HWND hwnd = (HWND)handle;
  if (!hwnd) {
    return;
  }

  // We need to get a minimize through.
  // Otherwise the SFW/PM hack won't work
  // However we need to protect against the watcher watching this
  HANDLE watch = ::GetPropW(hwnd, kWatch);
  ::SetPropW(hwnd, kWatch, (HANDLE)(0x2));
  ::ShowWindow(hwnd, SW_MINIMIZE);
  ::SetPropW(hwnd, kWatch, watch);

  ::ShowWindow(hwnd, SW_HIDE);
}
void WINAPI mintrayr_RestoreWindow(void *handle)
{
  HWND hwnd = (HWND)handle;
  if (!hwnd) {
    return;
  }

  // Show the window again
  ::ShowWindow(hwnd, SW_SHOW);

  // If it was minimized then restore it as well
  if (::IsIconic(hwnd)) {
    ::ShowWindow(hwnd, SW_RESTORE);
    // Try to grab focus
    ::SetForegroundWindow(hwnd);
  }
}

BOOL WINAPI mintrayr_CreateIcon(void *handle, mouseevent_callback_t callback)
{
  HWND hwnd = (HWND)handle;
  if (!hwnd) {
    return FALSE;
  }

  SetupWnd(hwnd);

  NOTIFYICONDATAW *iconData = new NOTIFYICONDATAW;
  // Init the icon data according to MSDN
  ZeroMemory(iconData, sizeof(NOTIFYICONDATAW));
  iconData->cbSize = sizeof(NOTIFYICONDATAW);

  // Copy the title
  if (GetWindowText(hwnd, iconData->szTip, 127)) {
    iconData->szTip[128] = '\0'; // Better be safe than sorry :p
  }
  else{
    iconData->szTip[0] = '\0';
  }

  // Get the window icon
  HICON icon = reinterpret_cast<HICON>(::SendMessageW(hwnd, WM_GETICON, ICON_SMALL, 0));
  if (icon == 0) {
    // Alternative method. Get from the window class
    icon = reinterpret_cast<HICON>(::GetClassLongPtrW(hwnd, GCLP_HICONSM));
  }
  // Alternative method: get the first icon from the main module (executable image of the process)
  if (icon == 0) {
    icon = ::LoadIcon(GetModuleHandleW(0), MAKEINTRESOURCE(0));
  }
  // Alternative method. Use OS default icon
  if (icon == 0) {
    icon = ::LoadIcon(0, IDI_APPLICATION);
  }
  iconData->hIcon = icon;

  // Set the rest of the members
  iconData->hWnd = hwnd;
  iconData->uCallbackMessage = WM_TRAYMESSAGE;
  iconData->uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
  iconData->uVersion = 5;

  // Install the icon
  ::Shell_NotifyIconW(NIM_ADD, iconData);
  ::Shell_NotifyIconW(NIM_SETVERSION, iconData);

  SetupWnd(hwnd);
  ::SetPropW(hwnd, kIconData, reinterpret_cast<HANDLE>(iconData));
  ::SetPropW(hwnd, kIconMouseEventProc, reinterpret_cast<HANDLE>(callback));
  ::SetPropW(hwnd, kIcon, reinterpret_cast<HANDLE>(0x1));

  return TRUE;
}

BOOL WINAPI mintrayr_DestroyIcon(void *handle)
{
  HWND hwnd = (HWND)handle;
  if (!hwnd) {
    return FALSE;
  }

  mintrayr_RestoreWindow(handle);

  SetupWnd(hwnd);
  ::RemovePropW(hwnd, kIcon);

  NOTIFYICONDATAW *iconData = reinterpret_cast<NOTIFYICONDATAW *>(::GetPropW(hwnd, kIconData));
  if (iconData) {
    ::Shell_NotifyIconW(NIM_DELETE, iconData);
    delete iconData;
  }
  ::RemovePropW(hwnd, kIconData);

  ::RemovePropW(hwnd, kIconMouseEventProc);

  return TRUE;
}

void* WINAPI mintrayr_GetBaseWindow(wchar_t *title)
{
	void *rv = 0;
	if (!title) {
		return rv;
	}
	rv = ::FindWindow(0, title);
	return rv;
}
