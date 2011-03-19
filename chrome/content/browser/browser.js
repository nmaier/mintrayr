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
 * The Original Code is MiniTrayR extension
 *
 * The Initial Developer of the Original Code is
 * Nils Maier.
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

var gMinTrayR = {};
addEventListener(
  'load',
  function() {
    function $(id) document.getElementById(id);
    removeEventListener("load", arguments.callee, true);

    if (!window.toolbar.visible) {
      return;
    }

    Components.utils.import("resource://mintrayr/mintrayr.jsm", gMinTrayR);
    gMinTrayR = new (function() {
      gMinTrayR.MinTrayR.call(
        this,
        $('MinTrayR_context'),
        'browser.watchbrowser'
        );
      this.cloneToMenu('MinTrayR_sep-top', ['menu_newNavigator'], false);
      this.cloneToMenu('MinTrayR_sep-bottom', ['menu_closeWindow', 'menu_FileQuitItem'], true);

      // Correctly tray-on-close, as Firefox 4 owner-draws the window controls in non-aero windows.
      // This will remove the commands from the close button widgets and add an own window handler.
      // Sucks, but there is not really another way.
      // See GH-6, GH-9
      (function(self) {
        function MinTrayRTryCloseToWindow(event){
          if (self.prefs.getExt('browser.watchbrowser', true)
            && (self.prefs.getExt('minimizeon', 1) & (1<<1))) {
            self.minimize();
            event.preventDefault();
            event.stopPropagation();
            return false;
          }
          // must be in sync with the original command
          return BrowserTryToCloseWindow();
        }
        function hijackCloseButton(id) {
          let closeButton = $(id);
          if (!closeButton) {
            // Only available in Firefox 4
            return;
          }

          // Remove old command(s)
          // titlebar-close sets both, command and oncommand :p
          closeButton.removeAttribute('command');
          closeButton.removeAttribute('oncommand');

          // Add ourselves
          closeButton.addEventListener('command', MinTrayRTryCloseToWindow, false);
        }
        ['titlebar-close', 'close-button'].forEach(hijackCloseButton);
      })(this);

    });
  },
  true
);
