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

typedef enum _eMinimizeActions {
	kTrayOnMinimize = (1 << 0),
	kTrayOnClose = (1 << 1)
} eMinimizeActions;

static
bool DoMinimizeWindow(nsIDOMWindow *window, eMinimizeActions action)
{
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

	nsresult rv;
	nsCOMPtr<trayITrayService> traySvc(do_GetService(TRAYSERVICE_CONTRACTID, &rv));
	if (NS_SUCCEEDED(rv)) {
		rv = traySvc->Minimize(window);
	}
	return NS_SUCCEEDED(rv);
}

/**
 * Helper: Get the base window for a DOM window
 */
static
NS_IMETHODIMP GetBaseWindow(nsIDOMWindow *aWindow, nsIBaseWindow **aBaseWindow)
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
 * Helper: Dispatch a trusted general event
 */
static
NS_IMETHODIMP DispatchTrustedEvent(nsIDOMWindow *aWindow, const nsAString& aEventName)
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
