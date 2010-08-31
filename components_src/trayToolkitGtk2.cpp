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

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>

#include <gtk/gtk.h>
#include <gdk/gdk.h>

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

#define XATOM(atom) static const Atom atom = XInternAtom(xev->xany.display, #atom, false)

/*
 * include common code
 */
namespace {

#include "common.cpp"

	/**
	 * Helper: Gdk filter function to "watch" the window
	 */
	static
	GdkFilterReturn TrayServiceImpl_filter(XEvent *xev, GdkEvent* event, nsIDOMWindow* window)
	{
		XATOM(WM_DELETE_WINDOW);

		if (!xev) {
			return GDK_FILTER_CONTINUE;
		}

		switch (xev->type) {
			case UnmapNotify:
				if (DoMinimizeWindow(window, kTrayOnMinimize)) {
					return GDK_FILTER_REMOVE;
				}
				break;

			case ClientMessage:
				if (xev->xclient.data.l
						&& static_cast<Atom>(xev->xclient.data.l[0]) == WM_DELETE_WINDOW
						&& DoMinimizeWindow(window, kTrayOnClose)
				) {
					return GDK_FILTER_REMOVE;
				}
				break;

			default:
				break;
		}
		return GDK_FILTER_CONTINUE;
	}
} // namespace


class TrayWindowWrapper {

private:
	TrayWindow *mWindow;

	GtkStatusIcon *mIcon;
	GtkWindow *mGtkWindow;
	GdkWindow *mGdkWindow;

private:
	void buttonEvent(GdkEventButton *event);
	static void gtkButtonEvent(GtkStatusIcon*, GdkEventButton *event, TrayWindowWrapper *wrapper) {
		wrapper->buttonEvent(event);
	}

	GdkFilterReturn filter(XEvent *xev, GdkEvent* event);
	static GdkFilterReturn gdkFilter(XEvent *xev, GdkEvent *event, TrayWindowWrapper *wrapper) {
		return wrapper->filter(xev, event);
	}

public:
	TrayWindowWrapper(TrayWindow *window, GdkWindow* gdkWindow, const nsString& Title);
	~TrayWindowWrapper();
};

TrayWindowWrapper::TrayWindowWrapper(TrayWindow *window, GdkWindow *gdkWindow, const nsString& aTitle)
	: mWindow(window), mGdkWindow(gdkWindow)
{
	GtkWidget *widget;
	gdk_window_get_user_data(mGdkWindow, reinterpret_cast<gpointer*>(&widget));
	widget = gtk_widget_get_toplevel(widget);
	mGtkWindow = reinterpret_cast<GtkWindow*>(widget);

	// Set up tray icon
	mIcon = gtk_status_icon_new();

	// Get the window icon and set it
	GdkPixbuf *buf = gtk_window_get_icon(mGtkWindow);
	if (buf) {
		gtk_status_icon_set_from_pixbuf(mIcon, buf);
	}

	// Get and set the title
	if (aTitle.IsEmpty()) {
		gtk_status_icon_set_tooltip_text(mIcon, gtk_window_get_title(mGtkWindow));
	}
	else {
		NS_ConvertUTF16toUTF8 titleUTF8(aTitle);
		gtk_status_icon_set_tooltip_text(mIcon, reinterpret_cast<const char*>(titleUTF8.get()));
	}

	// Add signals
	g_signal_connect(G_OBJECT(mIcon), "button-press-event", G_CALLBACK(gtkButtonEvent), this);
	g_signal_connect(G_OBJECT(mIcon), "button-release-event", G_CALLBACK(gtkButtonEvent), this);

	// Add filter
	gdk_window_add_filter(mGdkWindow, reinterpret_cast<GdkFilterFunc>(gdkFilter), this);

	// Hide the window
	gdk_window_hide(mGdkWindow);

	// Make visible
	gtk_status_icon_set_visible(mIcon, 1);
}

TrayWindowWrapper::~TrayWindowWrapper() {
	if (mIcon) {
		gtk_status_icon_set_visible(mIcon, 0);
		g_object_unref(mIcon);
	}
	gdk_window_remove_filter(mGdkWindow, reinterpret_cast<GdkFilterFunc>(gdkFilter), this);
	gdk_window_show(mGdkWindow);
}

void TrayWindowWrapper::buttonEvent(GdkEventButton *event)
{
	nsString eventName;
	switch (event->type) {
		case GDK_BUTTON_RELEASE: // use release, so that we don't duplicate events
			eventName = NS_LITERAL_STRING("TrayClick");
			break;
		case GDK_2BUTTON_PRESS:
			eventName = NS_LITERAL_STRING("TrayDblClick");
			break;
		case GDK_3BUTTON_PRESS:
			eventName = NS_LITERAL_STRING("TrayTriClick");
			break;
		default:
			return;
	}

	nsPoint pt((nscoord)(event->x + event->x_root), (nscoord)(event->y + event->y_root));

	// Dispatch the event
#define HASSTATE(x) (event->state & x ? PR_TRUE : PR_FALSE)
	mWindow->DispatchMouseEvent(
			eventName,
			event->button - 1,
			pt,
			HASSTATE(GDK_CONTROL_MASK),
			HASSTATE(GDK_MOD1_MASK),
			HASSTATE(GDK_SHIFT_MASK)
			);
#undef HASSTATE
}

GdkFilterReturn TrayWindowWrapper::filter(XEvent *xev, GdkEvent *event)
{
	XATOM(WM_DELETE_WINDOW);
	XATOM(WM_NAME);

	if (!xev) {
		return GDK_FILTER_CONTINUE;
	}

	switch (xev->type) {
	case VisibilityNotify:
		if (xev->xvisibility.state == VisibilityFullyObscured) {
			break;
		}
		mWindow->mService->Restore(mWindow->mDOMWindow);
		break;
	case MapNotify:
		// Restore
		mWindow->mService->Restore(mWindow->mDOMWindow);
		break;

	case ClientMessage:
		if (xev->xclient.data.l
				&& static_cast<Atom>(xev->xclient.data.l[0]) == WM_DELETE_WINDOW
		) {
			// Closed
			mWindow->mService->Restore(mWindow->mDOMWindow);
		}
		break;

	case PropertyNotify:
		if (xev->xproperty.atom == WM_NAME) {
			gtk_status_icon_set_tooltip_text(mIcon, gtk_window_get_title(mGtkWindow));
		}
		break;

	default:
		break;
	}
	return GDK_FILTER_CONTINUE;

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
	rv = GetBaseWindow(aWindow, getter_AddRefs(baseWindow));
	NS_ENSURE_SUCCESS(rv, rv);

	nativeWindow native = 0;
	rv = baseWindow->GetParentNativeWindow(&native);
	NS_ENSURE_SUCCESS(rv, rv);

	GdkWindow *window = gdk_window_get_toplevel(reinterpret_cast<GdkWindow*>(native));
	if (!window) {
		return NS_ERROR_UNEXPECTED;
	}

	nsString title;
	baseWindow->GetTitle(getter_Copies(title));

	mWrapper = new TrayWindowWrapper(this, window, title);
	mDOMWindow = aWindow;

	return NS_OK;
}

NS_IMPL_ISUPPORTS2(TrayServiceImpl, nsIObserver, trayITrayService)

TrayServiceImpl::TrayServiceImpl()
{
	Init();
}

TrayServiceImpl::~TrayServiceImpl()
{
	Destroy();
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
	rv = GetBaseWindow(aWindow, getter_AddRefs(baseWindow));
	NS_ENSURE_SUCCESS(rv, rv);

	nativeWindow native = 0;
	rv = baseWindow->GetParentNativeWindow(&native);
	NS_ENSURE_SUCCESS(rv, rv);

	GdkWindow *gdkWindow = gdk_window_get_toplevel(reinterpret_cast<GdkWindow*>(native));
	if (!gdkWindow) {
		return NS_ERROR_UNEXPECTED;
	}
	gdk_window_add_filter(gdkWindow, reinterpret_cast<GdkFilterFunc>(TrayServiceImpl_filter), aWindow);

	return NS_OK;
}
NS_IMETHODIMP TrayServiceImpl::UnwatchMinimize(nsIDOMWindow *aWindow)
{
	NS_ENSURE_ARG_POINTER(aWindow);

	nsresult rv;

	mWatches.RemoveObject(aWindow);

	nsCOMPtr<nsIBaseWindow> baseWindow;
	rv = GetBaseWindow(aWindow, getter_AddRefs(baseWindow));
	NS_ENSURE_SUCCESS(rv, rv);

	nativeWindow native = 0;
	rv = baseWindow->GetParentNativeWindow(&native);
	NS_ENSURE_SUCCESS(rv, rv);

	GdkWindow *gdkWindow = gdk_window_get_toplevel(reinterpret_cast<GdkWindow*>(native));
	if (!gdkWindow) {
		return NS_ERROR_UNEXPECTED;
	}
	gdk_window_remove_filter(gdkWindow, reinterpret_cast<GdkFilterFunc>(TrayServiceImpl_filter), aWindow);

	return NS_OK;
}

/*
 * include common trayToolkitImpl
 */
#include "trayToolkitImpl.cpp"
