/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is TrayToolkit
 *
 * The Initial Developer of the Original Code is
 * Nils Maier
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Nils Maier <MaierMan@web.de>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifdef _WIN32_IE
#	undef _WIN32_IE
#endif
#define _WIN32_IE 0x0600 // We want more features
#include <windows.h>
#include <shellapi.h>

#include "trayToolkit.h"

#include "nsCOMPtr.h"
#include "nsStringAPI.h"
#include "nsServiceManagerUtils.h"

#include "nsIDOMWindow.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDocumentView.h"
#include "nsIDOMAbstractView.h"

#include "nsIDOMDocumentEvent.h"
#include "nsIDOMEvent.h"
#include "nsIDOMEventTarget.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIDOMMouseEvent.h"

#include "nsIWebNavigation.h"
#include "nsIBaseWindow.h"

#include "nsIInterfaceRequestorUtils.h"
#include "nsIObserverService.h"

#include "nsIPrefService.h"
#include "nsIPrefBranch2.h"


namespace {

	static const wchar_t kTrayMessage[]    = L"_MINTRAYR_TrayMessageW";
	static const wchar_t kWrapperProp[]    = L"_MINTRAYR_WRAPPER_PTR";
	static const wchar_t kWrapperOldProp[] = L"_MINTRAYR_WRAPPER_OLD_PROC";
	static const wchar_t kWatcherDOMProp[] = L"_MINTRAYR_WATCHER_DOM_PTR";
	static const wchar_t kWatcherOldProp[] = L"_MINTRAYR_WATCHER_OLD_PROC";
	static const wchar_t kWatcherLock[]    = L"_MINTRAYR_WATCHER_LOCK";
	
	typedef enum _eMinimizeActions {
		kTrayOnMinimize = (1 << 0),
		kTrayOnClose = (1 << 1)
	} eMinimizeActions;

	typedef BOOL (WINAPI *pChangeWindowMessageFilter)(UINT message, DWORD dwFlag);
#ifndef MGSFLT_ADD
	// Not a Vista SDK
	#	define MSGFLT_ADD 1
	#	define MSGFLT_REMOVE 2
#endif
 
	static UINT WM_TASKBARCREATED = 0;
	static UINT WM_TRAYMESSAGE = 0;

	/**
	 * Helper function that will allow us to receive some broadcast messages on Vista 
	 * (We need to bypass that filter if we run as Administrator, but the orginating process
	 * has less priviledges)
	 */
	static void TrayServiceImpl_AdjustMessageFilters(UINT filter) 
	{
		HMODULE user32 = LoadLibraryW(L"user32.dll");
		if (user32 != NULL) {
			pChangeWindowMessageFilter changeWindowMessageFilter =
				reinterpret_cast<pChangeWindowMessageFilter>(GetProcAddress(
					user32,
					"ChangeWindowMessageFilter"
				));
			if (changeWindowMessageFilter != NULL) {
				changeWindowMessageFilter(WM_TASKBARCREATED, filter);
			}
			FreeLibrary(user32);
		}
	}

	/**
	 * Helper function that will try to get the nsIBaseWindow from an nsIDOMWindow
	 */
	static NS_IMETHODIMP TrayServiceImpl_GetBaseWindow(nsIDOMWindow *aWindow, nsIBaseWindow **aBaseWindow)
	{
		NS_ENSURE_ARG_POINTER(aWindow);
		NS_ENSURE_ARG_POINTER(aBaseWindow);

		nsresult rv;
		
		nsCOMPtr<nsIWebNavigation> webNav = do_GetInterface(aWindow, &rv);
		NS_ENSURE_SUCCESS(rv, rv);
		
		nsCOMPtr<nsIBaseWindow> baseWindow = do_QueryInterface(webNav, &rv);
		NS_ENSURE_SUCCESS(rv, rv);

		*aBaseWindow = baseWindow;
		NS_IF_ADDREF(*aBaseWindow);
		return NS_OK;
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
	
	static bool DoMinimizeWindow(HWND hwnd, eMinimizeActions action)
	{
		nsIDOMWindow *window = reinterpret_cast<nsIDOMWindow*>(::GetPropW(hwnd, kWatcherDOMProp));
		if (window == 0) {			
			return false;
		}
		
		nsCOMPtr<nsIPrefBranch2> prefs(do_GetService(NS_PREFSERVICE_CONTRACTID));
		if (prefs) {
			PRInt32 whenToMinimize = 0;
			prefs->GetIntPref("extensions.mintrayr.minimizeon", &whenToMinimize);
			if ((whenToMinimize & action) == 0) {
				return false;
			}
		}

		// We might receive the hide message, protect against it.
		if (reinterpret_cast<unsigned int>(::GetPropW(hwnd, kWatcherLock)) == 0x1) {
			return false;
		}
		::SetPropW(hwnd, kWatcherLock, reinterpret_cast<HANDLE>(0x1));

		nsresult rv;
		nsCOMPtr<trayITrayService> traySvc(do_GetService(TRAYSERVICE_CONTRACTID, &rv));
		if (NS_SUCCEEDED(rv)) {
			rv = traySvc->Minimize(window);
		}

		::RemovePropW(hwnd, kWatcherLock);	
		return NS_SUCCEEDED(rv);
	}

	/**
	 * Install this WindowProc in windows we watch
	 * 
	 * Note: Once installed don't remove this again (by setting the old WNDPROC)
	 * Or else we might break the chain if something else installed yet another WNDPROC
	 * And chained us.
	 * Only the property kWatcherDOMProp should be removed, so that the function knows when
	 * to handle stuff and when to simply pass it down the chain.
	 */
	LRESULT CALLBACK TrayServiceImpl_WatchWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		nsIDOMWindow *window = reinterpret_cast<nsIDOMWindow*>(::GetPropW(hwnd, kWatcherDOMProp));
		if (window == 0) {
			goto WatcherWindowProcEnd;
		}
	
		switch (uMsg) {
		case WM_CLOSE:
		case WM_DESTROY: {
			// The window is (about to be) destroyed
			// Let the service know and unwatch the window
			nsresult rv;
			nsCOMPtr<trayITrayService> traySvc(do_GetService(TRAYSERVICE_CONTRACTID, &rv));
			if (NS_SUCCEEDED(rv)) {
				traySvc->UnwatchMinimize(window);
			}
			break;
		} // case WM_DESTROY
		
		case WM_WINDOWPOSCHANGED: {
			/* XXX Fix this bit to something more reasonable
				The following code kinda replicates the way mozilla gets the window state.
				We intensionally "hide" the SW_SHOWMINIMIZED here.
				This indeed might cause some side effects, but if it didn't we couldn't open
				menus due to bugzilla #435848,.
				This might defeat said bugfix completely reverting to old behavior, but only when we're active, of course.
			*/
			WINDOWPOS *wp = reinterpret_cast<WINDOWPOS*>(lParam);
			if (wp->flags & SWP_FRAMECHANGED && ::IsWindowVisible(hwnd)) {
				WINDOWPLACEMENT pl;
				pl.length = sizeof(WINDOWPLACEMENT);
				::GetWindowPlacement(hwnd, &pl);
				if (pl.showCmd == SW_SHOWMINIMIZED) {
					if (DoMinimizeWindow(hwnd, kTrayOnMinimize)) {
						pl.showCmd = SW_HIDE;
						::SetWindowPlacement(hwnd, &pl);
					}
				}
			}
			break;
		} // case WM_WINDOWPOSCHANGED
		
		case WM_NCLBUTTONDOWN:
		case WM_NCLBUTTONUP:
			if (wParam == HTCLOSE && DoMinimizeWindow(hwnd, kTrayOnClose)) {
				return TRUE;
			}
			break;
		}

WatcherWindowProcEnd:
		// Call the old WNDPROC or at lest DefWindowProc
		WNDPROC oldProc = reinterpret_cast<WNDPROC>(::GetPropW(hwnd, kWatcherOldProp));
		if (oldProc != NULL) {
			return ::CallWindowProcW(oldProc, hwnd, uMsg, wParam, lParam);
		}
		return ::DefWindowProcW(hwnd, uMsg, wParam, lParam);
	}
}

/**
 * Helper class
 * Encapsulates the Windows specific initialization code and message processing
 */
class TrayWindowWrapper {
public:
	static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

public:
	TrayWindowWrapper(TrayWindow *window, HWND hwnd, const wchar_t *title);
	~TrayWindowWrapper();

private:
	 // hold raw pointer. Class instances point to TrayWindow anyway and destructed with it.
	TrayWindow* mWindow;
	
	// HWND this wrapper is associated with
	HWND mHwnd;
	
	// Previous window procedure
	WNDPROC mOldWindowProc;
	
	// Win32 data structure containing the tray icon configuration
	NOTIFYICONDATAW mIconData;
};


TrayWindowWrapper::TrayWindowWrapper(TrayWindow *aWindow, HWND aHwnd, const wchar_t *aTitle)
	: mHwnd(aHwnd), mWindow(aWindow)
{
	// Hook window
	::SetPropW(mHwnd, kWrapperProp, this);
	
	// Only subclass once and leave this intact
	if (::GetPropW(mHwnd, kWrapperOldProp) == NULL) {
		WNDPROC oldWindowProc = reinterpret_cast<WNDPROC>(::SetWindowLongPtrW(
			mHwnd,
			GWLP_WNDPROC,
			(LONG_PTR)(TrayWindowWrapper::WindowProc)
		));
		::SetPropW(mHwnd, kWrapperOldProp, oldWindowProc);
	}
	
	// We need to get a minimize through.
	// Otherwise the SFW/PM hack won't work
	// However we need to protect against the watcher watching this
	// XXX: Do it properly
	void *watcher = reinterpret_cast<void*>(::GetPropW(mHwnd, kWatcherDOMProp));
	::RemovePropW(mHwnd, kWatcherDOMProp);
	::ShowWindow(mHwnd, SW_MINIMIZE);
	if (watcher != 0) {
		::SetPropW(mHwnd, kWatcherDOMProp, watcher);
	}	

	// Init the icon data according to MSDN
	ZeroMemory(&mIconData, sizeof(mIconData));
	mIconData.cbSize = sizeof(mIconData);

	// Copy the title
	lstrcpynW(mIconData.szTip, aTitle, 127);
	mIconData.szTip[128] = '\0'; // Better be safe than sorry :p

	// Get the window icon
	HICON icon = reinterpret_cast<HICON>(::SendMessageW(mHwnd, WM_GETICON, ICON_SMALL, NULL));
	if (icon == NULL) {
		// Alternative method. Get from the window class
		icon = reinterpret_cast<HICON>(::GetClassLongPtrW(mHwnd, GCLP_HICONSM));
	}
	// Alternative method: get the first icon from the main module (executable image of the process)
	if (icon == NULL) {
		icon = ::LoadIcon(GetModuleHandleW(NULL), MAKEINTRESOURCE(0));
	}
	// Alternative method. Use OS default icon
	if (icon == NULL) {
		icon = ::LoadIcon(NULL, IDI_APPLICATION);
	}
	mIconData.hIcon = icon;
	
	// Set the rest of the members
	mIconData.hWnd = mHwnd;
	mIconData.uCallbackMessage = WM_TRAYMESSAGE;
	mIconData.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
	mIconData.uVersion = 5;

	// Install the icon
	if (::Shell_NotifyIconW(NIM_ADD, &mIconData)) {
		::ShowWindow(mHwnd, SW_HIDE);
	}
	::Shell_NotifyIconW(NIM_SETVERSION, &mIconData);
}
TrayWindowWrapper::~TrayWindowWrapper()
{
	// Remove kWrapperProp, so that the WindowProc knows not to handle any messages
	::RemovePropW(mHwnd, kWrapperProp);

	// Remove the icon
	::Shell_NotifyIconW(NIM_DELETE, &mIconData);

	// Show the window again
	::ShowWindow(mHwnd, SW_SHOW);

	// If it was minimized then restore it as well
	if (::IsIconic(mHwnd)) {
		::ShowWindow(mHwnd, SW_RESTORE);
	}

	// Try to grab focus
	::SetForegroundWindow(mHwnd);
}

LRESULT CALLBACK TrayWindowWrapper::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	TrayWindowWrapper *me = reinterpret_cast<TrayWindowWrapper*>(GetPropW(hwnd, kWrapperProp));

	if (me == 0) {
		// We should not process. Directly jump to default message passing
		goto WrapperWindowProcEnd;
	}

	// Do not switch messages; not constant
	if (uMsg == WM_CLOSE || uMsg == WM_DESTROY) {
		// Got closed, no point in staying alive
		me->mWindow->mService->Restore(me->mWindow->mDOMWindow);
	}

	// The window might have been restored by someone else
	else if (uMsg == WM_WINDOWPOSCHANGED && me->mHwnd == hwnd) {
		// Check if somebody else tries to show us again
		WINDOWPOS *pos = reinterpret_cast<WINDOWPOS*>(lParam);
		if (pos && pos->flags & SWP_SHOWWINDOW) {
			// shown again, unexpectedly that is, so release
			me->mWindow->mService->Restore(me->mWindow->mDOMWindow);
		}
		/* XXX Fix this bit to something more reasonable
			The following code kinda replicates the way mozilla gets the window state.
			We intensionally "hide" the SW_SHOWMINIMIZED here.
			This indeed might cause some side effects, but if it didn't we couldn't open
			menus due to bugzilla #435848,.
			This might defeat said bugfix completely reverting to old behavior, but only when we're active, of course.
		*/
		if (pos->flags & SWP_FRAMECHANGED && ::IsWindowVisible(hwnd)) {
			WINDOWPLACEMENT pl;
			pl.length = sizeof(WINDOWPLACEMENT);
			::GetWindowPlacement(hwnd, &pl);
			if (pl.showCmd == SW_SHOWMINIMIZED) {
				pl.showCmd = SW_HIDE;
				::SetWindowPlacement(hwnd, &pl);
			}
		}
	}
	
	// This is a badly documented custom broadcast message by explorer
	else if (uMsg == WM_TASKBARCREATED) {
		// The taskbar was (re)created. Add ourselves again.
		Shell_NotifyIconW(NIM_ADD, &me->mIconData);
	}
	
	// We got clicked. How exciting, isn't it.
	else if (uMsg == WM_TRAYMESSAGE) {
		nsString eventName;
		PRUint16 button = 0;
		switch (LOWORD(lParam)) {
			case WM_LBUTTONUP:
			case WM_MBUTTONUP:
			case WM_RBUTTONUP:
			case WM_CONTEXTMENU:
			case NIN_KEYSELECT:
				eventName = NS_LITERAL_STRING("TrayClick");
				break;
			case WM_LBUTTONDBLCLK:
			case WM_MBUTTONDBLCLK:
			case WM_RBUTTONDBLCLK:
				eventName = NS_LITERAL_STRING("TrayDblClick");
				break;
		}
		switch (LOWORD(lParam)) {
			case WM_LBUTTONUP:
			case WM_LBUTTONDBLCLK:
				button = 0;
				break;
			case WM_MBUTTONUP:
			case WM_MBUTTONDBLCLK:
				button = 1;
				break;
			case WM_RBUTTONUP:
			case WM_RBUTTONDBLCLK:
			case WM_CONTEXTMENU:
			case NIN_KEYSELECT:
				button = 2;
				break;
		}
		if (eventName.IsEmpty() == PR_FALSE) {
			POINT wpt;
			if (GetCursorPos(&wpt) == TRUE) {
				nsPoint pt((nscoord)wpt.x, (nscoord)wpt.y);

				PRBool ctrlKey = (::GetKeyState(VK_CONTROL) & 0x8000) != 0;
				PRBool altKey = (::GetKeyState(VK_MENU) & 0x8000) != 0;
				PRBool shiftKey = (::GetKeyState(VK_SHIFT) & 0x8000) != 0;

				// SFW/PM is a win32 hack, so that the context menu is hidden when loosing focus.
				::SetForegroundWindow(hwnd);
				me->mWindow->DispatchMouseEvent(eventName, button, pt, ctrlKey, altKey, shiftKey);
				::PostMessage(me->mHwnd, WM_NULL, 0, 0L);
			}
		}
		return 0;
	}
	
	// Window title changed
	else if (uMsg == WM_SETTEXT) {
		// First, let the original wndproc process this message,
		// so that we may query the thing afterwards ;)
		// this is required because we cannot know the encoding of this message for sure ;)
		LRESULT rv;
		WNDPROC oldWindowProc = reinterpret_cast<WNDPROC>(::GetPropW(hwnd, kWrapperOldProp));
		if (oldWindowProc != NULL) {
			rv = CallWindowProcW(oldWindowProc, hwnd, uMsg, wParam, lParam);
		}
		else {
			rv = DefWindowProcW(hwnd, uMsg, wParam, lParam);
		}

		if (::GetWindowTextW(hwnd, me->mIconData.szTip, 128) != 0) {
			me->mIconData.szTip[128] = '\0';
			Shell_NotifyIconW(NIM_MODIFY, &me->mIconData);
		}
		return rv;
	}

WrapperWindowProcEnd:
	// Default processing
	WNDPROC oldWindowProc = reinterpret_cast<WNDPROC>(::GetPropW(hwnd, kWrapperOldProp));
	if (oldWindowProc != NULL) {
		return CallWindowProcW(oldWindowProc, hwnd, uMsg, wParam, lParam);
	}
	return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

NS_IMPL_ISUPPORTS0(TrayWindow)

TrayWindow::TrayWindow(TrayServiceImpl *aService)
: mService(aService), mWrapper(nsnull), mDOMWindow(nsnull)
{
}

NS_IMETHODIMP TrayWindow::Destroy()
{
	// Deleting the wrapper will make it destroy any icon as well
	delete mWrapper.forget();

	return NS_OK;
}

NS_IMETHODIMP TrayWindow::Init(nsIDOMWindow *aWindow)
{
	NS_ENSURE_ARG_POINTER(aWindow);

	nsresult rv;

	nsCOMPtr<nsIBaseWindow> baseWindow;
	rv = TrayServiceImpl_GetBaseWindow(aWindow, getter_AddRefs(baseWindow));
	NS_ENSURE_SUCCESS(rv, rv);

	nativeWindow native = 0;
	rv = baseWindow->GetParentNativeWindow(&native);
	NS_ENSURE_SUCCESS(rv, rv);
	
	HWND hwnd = reinterpret_cast<HWND>(native);

	PRUnichar *title = nsnull;

	baseWindow->GetTitle(&title);

	mWrapper = new TrayWindowWrapper(this, hwnd, title);
	mDOMWindow = aWindow;

	NS_Free(title);

	return NS_OK;
}

NS_IMETHODIMP TrayWindow::GetWindow(nsIDOMWindow **aWindow)
{
	NS_ENSURE_ARG_POINTER(aWindow);
	*aWindow = mDOMWindow;
	NS_IF_ADDREF(*aWindow);

	return NS_OK;
}

NS_IMETHODIMP TrayWindow::DispatchMouseEvent(const nsAString& aEventName, PRUint16 aButton, nsPoint& pt, PRBool aCtrlKey, PRBool aAltKey, PRBool aShiftKey)
{
	nsresult rv;

	nsCOMPtr<nsIDOMDocument> domDocument;
	rv = mDOMWindow->GetDocument(getter_AddRefs(domDocument));
	NS_ENSURE_SUCCESS(rv, rv);

	nsCOMPtr<nsIDOMDocumentEvent> docEvent(do_QueryInterface(domDocument));
	nsCOMPtr<nsIDOMEventTarget> target(do_QueryInterface(domDocument));
	NS_ENSURE_TRUE(docEvent && target, NS_ERROR_INVALID_ARG);

	nsCOMPtr<nsIDOMDocumentView> docView(do_QueryInterface(domDocument, &rv));
	NS_ENSURE_SUCCESS(rv, rv);
	
	nsCOMPtr<nsIDOMAbstractView> abstractView; 
	rv = docView->GetDefaultView(getter_AddRefs(abstractView));
	NS_ENSURE_SUCCESS(rv, rv);

	nsCOMPtr<nsIDOMEvent> event;
	rv = docEvent->CreateEvent(NS_LITERAL_STRING("MouseEvents"), getter_AddRefs(event));
	NS_ENSURE_SUCCESS(rv, rv);

	nsCOMPtr<nsIDOMMouseEvent> mouseEvent(do_QueryInterface(event, &rv));
	NS_ENSURE_SUCCESS(rv, rv);

	rv = mouseEvent->InitMouseEvent(
		aEventName,
		PR_FALSE,
		PR_TRUE,
		abstractView,
		0,
		pt.x,
		pt.y,
		0,
		0,
		aCtrlKey,
		aAltKey,
		aShiftKey,
		PR_FALSE,
		aButton,
		target
		);
	NS_ENSURE_SUCCESS(rv, rv);

	PRBool dummy;
	return target->DispatchEvent(mouseEvent, &dummy);
}

NS_IMPL_ISUPPORTS2(TrayServiceImpl, nsIObserver, trayITrayService)

TrayServiceImpl::TrayServiceImpl()
{
	// Register our hidden window
	// Get TaskbarCreated
	// OK, this is doubled, but doesn't matter
	WM_TASKBARCREATED = RegisterWindowMessageW(L"TaskbarCreated");
	// We register this as well, as we cannot know which WM_USER values are already taken
	WM_TRAYMESSAGE = RegisterWindowMessageW(kTrayMessage);
	
	// Vista (Administrator) needs some love, or else we won't receive anything due to UIPI
	if (OSVersionInfo().isVistaOrLater()) {
		TrayServiceImpl_AdjustMessageFilters(MSGFLT_ADD);
	}

	// Observe when the app is going down.
	// Else we might not properly clean up
	// And leave some tray icons behind
	nsresult rv;
	nsCOMPtr<nsIObserverService> obs(do_GetService("@mozilla.org/observer-service;1", &rv));
	if (NS_SUCCEEDED(rv)) {
		obs->AddObserver(static_cast<nsIObserver*>(this), "xpcom-shutdown", PR_FALSE);
	}
	
}
TrayServiceImpl::~TrayServiceImpl()
{
	Destroy();
}
void TrayServiceImpl::Destroy()
{
	// Destroy remaining icons
	PRInt32 count = mWindows.Count();
	for (PRInt32 i = count - 1; i > -1; --i) {
		mWindows[i]->Destroy();
		ReleaseTrayWindow(mWindows[i]);
	}
	mWindows.Clear();
	mWatches.Clear();

	// Vista (Administrator) needs some unlove, see c'tor
	if (OSVersionInfo().isVistaOrLater()) {
		TrayServiceImpl_AdjustMessageFilters(MSGFLT_REMOVE);
	}
}

NS_IMETHODIMP TrayServiceImpl::Minimize(nsIDOMWindow *aWindow)
{
	NS_ENSURE_ARG_POINTER(aWindow);
	
	nsresult rv;
	nsCOMPtr<TrayWindow> trayWindow;
	rv = FindTrayWindow(aWindow, getter_AddRefs(trayWindow));
	if (NS_SUCCEEDED(rv)) {
		return NS_ERROR_ALREADY_INITIALIZED;
	}

	trayWindow = new TrayWindow(this);
	if (trayWindow == nsnull) {
		return NS_ERROR_OUT_OF_MEMORY;
	}
	rv = trayWindow->Init(aWindow);
	NS_ENSURE_SUCCESS(rv, rv);

	if (mWindows.AppendObject(trayWindow) == PR_FALSE) {
		return NS_ERROR_FAILURE;
	}
	DispatchTrustedEvent(aWindow, NS_LITERAL_STRING("TrayMinimize"));
	return NS_OK;
}

NS_IMETHODIMP TrayServiceImpl::Restore(nsIDOMWindow *aWindow)
{
	NS_ENSURE_ARG_POINTER(aWindow);

	nsresult rv;
	nsCOMPtr<TrayWindow> trayWindow;
	rv = FindTrayWindow(aWindow, getter_AddRefs(trayWindow));
	if (NS_FAILED(rv)) {
		return NS_OK;
	}

	rv = trayWindow->Destroy();
	NS_ENSURE_SUCCESS(rv, rv);

	rv = ReleaseTrayWindow(trayWindow);
	NS_ENSURE_SUCCESS(rv, rv);

	DispatchTrustedEvent(aWindow, NS_LITERAL_STRING("TrayRestore"));

	return NS_OK;
}

NS_IMETHODIMP TrayServiceImpl::RestoreAll()
{
	nsresult rv;
	PRInt32 count = mWindows.Count();

	for (PRInt32 i = count - 1; i > -1; --i) {
		
		nsCOMPtr<nsIDOMWindow> window;
		rv = mWindows[i]->GetWindow(getter_AddRefs(window));
		NS_ENSURE_SUCCESS(rv, rv);

		rv = mWindows[i]->Destroy();
		NS_ENSURE_SUCCESS(rv, rv);

		rv = ReleaseTrayWindow(mWindows[i]);
		NS_ENSURE_SUCCESS(rv, rv);

		DispatchTrustedEvent(window, NS_LITERAL_STRING("TrayRestore"));
	}
    return NS_OK;
}

NS_IMETHODIMP TrayServiceImpl::WatchMinimize(nsIDOMWindow *aWindow)
{
	NS_ENSURE_ARG_POINTER(aWindow);

	PRInt32 index = mWatches.IndexOf(aWindow);
	if (index != -1) {
		return NS_OK;
	}

	nsresult rv;

	nsCOMPtr<nsIBaseWindow> baseWindow;
	rv = TrayServiceImpl_GetBaseWindow(aWindow, getter_AddRefs(baseWindow));
	NS_ENSURE_SUCCESS(rv, rv);

	nativeWindow native = 0;
	rv = baseWindow->GetParentNativeWindow(&native);
	NS_ENSURE_SUCCESS(rv, rv);
	
	HWND hwnd = reinterpret_cast<HWND>(native);

	// Storing this as raw pointer shouldn't be a problem, as the lifetime of the DOMWindow is at least as long as of the BaseWindow
	::SetPropW(hwnd, kWatcherDOMProp, reinterpret_cast<HANDLE>(aWindow));

	// We only subclass once
	if (::GetPropW(hwnd, kWatcherOldProp) == NULL) {
		WNDPROC oldProc = reinterpret_cast<WNDPROC>(::SetWindowLongPtrW(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(TrayServiceImpl_WatchWindowProc)));
		::SetPropW(hwnd, kWatcherOldProp, reinterpret_cast<HANDLE>(oldProc));
	}

	return NS_OK;
}
NS_IMETHODIMP TrayServiceImpl::UnwatchMinimize(nsIDOMWindow *aWindow)
{
	NS_ENSURE_ARG_POINTER(aWindow);

	nsresult rv;

	mWatches.RemoveObject(aWindow);

	nsCOMPtr<nsIBaseWindow> baseWindow;
	rv = TrayServiceImpl_GetBaseWindow(aWindow, getter_AddRefs(baseWindow));
	NS_ENSURE_SUCCESS(rv, rv);

	nativeWindow native = 0;
	rv = baseWindow->GetParentNativeWindow(&native);
	NS_ENSURE_SUCCESS(rv, rv);
	
	HWND hwnd = reinterpret_cast<HWND>(native);

	// This will effectively cause the WindowProc not to carry out anything itself.
	// We cannot simply remove that window proc, as somebody else might have put another in and chained us
	::RemovePropW(hwnd, kWatcherDOMProp);


	return NS_OK;
}

NS_IMETHODIMP TrayServiceImpl::IsWatchedWindow(nsIDOMWindow *aWindow, PRBool *aResult)
{
	NS_ENSURE_ARG_POINTER(aWindow);
	NS_ENSURE_ARG_POINTER(aResult);
	
	*aResult = mWatches.IndexOfObject(aWindow) != -1 ? PR_TRUE : PR_FALSE;
	return NS_OK;
}

NS_IMETHODIMP TrayServiceImpl::Observe(nsISupports *, const char *aTopic, const PRUnichar *)
{
	if (lstrcmpA(aTopic, "xpcom-shutdown") == 0) {
		Destroy();

		nsresult rv;
		nsCOMPtr<nsIObserverService> obs(do_GetService("@mozilla.org/observer-service;1", &rv));
		if (NS_SUCCEEDED(rv)) {
			obs->RemoveObserver(static_cast<nsIObserver*>(this), "xpcom-shutdown");
		}
	}
	return NS_OK;
}

NS_IMETHODIMP TrayServiceImpl::ReleaseTrayWindow(TrayWindow *aWindow)
{
	NS_ENSURE_ARG_POINTER(aWindow);
	mWindows.RemoveObject(aWindow);
	return NS_OK;
}


NS_IMETHODIMP TrayServiceImpl::FindTrayWindow(nsIDOMWindow *aWindow, TrayWindow **aTrayWindow)
{
	NS_ENSURE_ARG_POINTER(aWindow);
	NS_ENSURE_ARG_POINTER(aTrayWindow);
	
	nsresult rv;
	nsCOMPtr<nsIDOMWindow> domWindow;
	PRInt32 count = mWindows.Count();
	
	for (PRInt32 i = 0; i < count; ++i) {
		rv = mWindows[i]->GetWindow(getter_AddRefs(domWindow));
		if (NS_FAILED(rv)) {
			continue;
		}
		if (domWindow == aWindow) {
			*aTrayWindow = mWindows[i];
			NS_IF_ADDREF(*aTrayWindow);
			return NS_OK;
		}
	}
	return NS_ERROR_FAILURE;
}

NS_IMETHODIMP TrayServiceImpl::DispatchTrustedEvent(nsIDOMWindow *aWindow, const nsAString& aEventName)
{
	NS_ENSURE_ARG_POINTER(aWindow);

	nsresult rv;

	nsCOMPtr<nsIDOMDocument> domDocument;
	rv = aWindow->GetDocument(getter_AddRefs(domDocument));
	NS_ENSURE_SUCCESS(rv, rv);

	nsCOMPtr<nsIDOMDocumentEvent> docEvent(do_QueryInterface(domDocument));
	nsCOMPtr<nsIDOMEventTarget> target(do_QueryInterface(domDocument));
	NS_ENSURE_TRUE(docEvent && target, NS_ERROR_INVALID_ARG);

	nsCOMPtr<nsIDOMEvent> event;
	rv = docEvent->CreateEvent(NS_LITERAL_STRING("Events"), getter_AddRefs(event));
	NS_ENSURE_SUCCESS(rv, rv);
	
	nsCOMPtr<nsIPrivateDOMEvent> privateEvent(do_QueryInterface(event, &rv));
	NS_ENSURE_SUCCESS(rv, rv);
	
	rv = event->InitEvent(aEventName, PR_FALSE, PR_TRUE);
	NS_ENSURE_SUCCESS(rv, rv);
	
	rv = privateEvent->SetTrusted(PR_TRUE);
	NS_ENSURE_SUCCESS(rv, rv);
 
	PRBool dummy;
	return target->DispatchEvent(event, &dummy);
}
