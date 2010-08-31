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

/*
 * This is just an include ;)
 */

NS_IMPL_ISUPPORTS2(TrayServiceImpl, nsIObserver, trayITrayService)

void TrayServiceImpl::Init()
{
	// Observe when the app is going down.
	// Else we might not properly clean up
	// And leave some tray icons behind
	nsresult rv;
	nsCOMPtr<nsIObserverService> obs(do_GetService("@mozilla.org/observer-service;1", &rv));
	if (NS_SUCCEEDED(rv)) {
		obs->AddObserver(static_cast<nsIObserver*>(this), "xpcom-shutdown", PR_FALSE);
	}
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

NS_IMETHODIMP TrayWindow::GetWindow(nsIDOMWindow **aWindow)
{
	NS_ENSURE_ARG_POINTER(aWindow);
	*aWindow = mDOMWindow;
	NS_IF_ADDREF(*aWindow);

	return NS_OK;
}

inline
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

NS_IMETHODIMP TrayServiceImpl::IsWatchedWindow(nsIDOMWindow *aWindow, PRBool *aResult)
{
	NS_ENSURE_ARG_POINTER(aWindow);
	NS_ENSURE_ARG_POINTER(aResult);

	*aResult = mWatches.IndexOfObject(aWindow) != -1 ? PR_TRUE : PR_FALSE;
	return NS_OK;
}

NS_IMETHODIMP TrayServiceImpl::Observe(nsISupports *, const char *aTopic, const PRUnichar *)
{
	if (strcasecmp(aTopic, "xpcom-shutdown") == 0) {
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