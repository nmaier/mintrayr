/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/ */

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
    gMinTrayR = new gMinTrayR.MinTrayR($('MinTrayR_context'), 'browser.watchbrowser', function() {
      this.cloneToMenu('MinTrayR_sep-top', ['menu_newNavigator'], false);
      this.cloneToMenu('MinTrayR_sep-bottom', ['menu_closeWindow', 'menu_FileQuitItem'], true);

      // Correctly tray-on-close, as Firefox 4 owner-draws the window controls in non-aero windows.
      // This will remove the commands from the close button widgets and add an own window handler.
      // Sucks, but there is not really another way.
      // See GH-6, GH-9
      (function(self) {
        function MinTrayRTryCloseWindow(event){
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
        function MinTrayRTryMinimizeWindow(event) {
          if (self.prefs.getExt('browser.watchbrowser', true)
              && (self.prefs.getExt('minimizeon', 1) & (1<<0))) {
              self.minimize();
              event.preventDefault();
              event.stopPropagation();
              return false;
            }
            // must be in sync with the original command
            return window.minimize();
        }

        function hijackButton(newCommand, id) {
          let button = $(id);
          if (!button) {
            // Only available in Firefox 4
            return;
          }

          // Remove old command(s)
          // titlebar-close sets both, command and oncommand :p
          button.removeAttribute('command');
          button.removeAttribute('oncommand');

          // Add ourselves
          button.addEventListener('command', newCommand, false);
        }
        ['titlebar-close', 'close-button'].forEach(hijackButton.bind(null, MinTrayRTryCloseWindow));
        ['titlebar-min', 'minimize-button'].forEach(hijackButton.bind(null, MinTrayRTryMinimizeWindow));
      })(this);

      // Need to hook BrowserTryToCloseWindow in order for session restore
      // to not get wrong window coords.
      // See GH-84, GH-89
      (function hookSessionStore(self) {

        const o = BrowserTryToCloseWindow;
        BrowserTryToCloseWindow = function mintrayr_BrowserTryToCloseWindow() {
          try {
            self.restore();
          }
          catch (ex) {}

          return o.apply(null, arguments)
        };
      })(this);
    });
  },
  true
);
