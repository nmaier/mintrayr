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

#ifndef __TRAYTOOLKIT_IMPL_H
#define __TRAYTOOLKIT_IMPL_H

#include "mozilla-config.h"
#include "xpcom-config.h"

#include "nsCOMArray.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsPoint.h"

#include "nsIDOMEventListener.h"

#include "trayIToolkit.h"
#include "nsXPCOMStrings.h"
#include "nsIObserver.h"


#define TRAYSERVICE_CONTRACTID "@tn123.ath.cx/trayservice;1"
#define TRAYSERVICE_CLASSNAME  "TrayServiceImpl"

class TrayWindowWrapper;
class TrayServiceImpl;

class TrayWindow : public nsISupports {

	friend class TrayWindowWrapper;

public:
	NS_DECL_ISUPPORTS

	TrayWindow(TrayServiceImpl *service);

	NS_IMETHOD Init(nsIDOMWindow *Window);
	NS_IMETHOD GetWindow(nsIDOMWindow **Window);
	NS_IMETHOD Destroy();
	NS_IMETHOD DispatchMouseEvent(const nsAString& eventName, PRUint16 button, nsPoint& pt, PRBool ctrlKey, PRBool altKey, PRBool shiftKey);

private:
	// raw to resolve ambiguous conversation; doesn't hurt, all TrayWindow instanced should be down before TrayService goes away
	TrayServiceImpl* mService;
	nsAutoPtr<TrayWindowWrapper> mWrapper;
	nsCOMPtr<nsIDOMWindow> mDOMWindow;
};

class TrayServiceImpl : public trayITrayService, nsIDOMEventListener, nsIObserver {

	friend class TrayWindow;
public:
	NS_DECL_ISUPPORTS
	NS_DECL_NSIOBSERVER
	NS_DECL_NSIDOMEVENTLISTENER
	NS_DECL_TRAYITRAYSERVICE

	TrayServiceImpl();
private:
	nsCOMArray<TrayWindow> mWindows;
	nsCOMArray<nsIDOMWindow> mWatches;

private:
	~TrayServiceImpl();

	void Init();
	void Destroy();
	NS_IMETHOD FindTrayWindow(nsIDOMWindow *window, TrayWindow **trayWindow);
	NS_IMETHOD ReleaseTrayWindow(TrayWindow *window);
};

#endif
