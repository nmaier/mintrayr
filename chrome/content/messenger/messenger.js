/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/ */

var gMinTrayR = {};
addEventListener(
  "load",
  function() {
    function $(id) {
      return document.getElementById(id);
    }
    removeEventListener("load", arguments.callee, true);

    Components.utils.import("resource://mintrayr/mintrayr.jsm", gMinTrayR);
    gMinTrayR = new gMinTrayR.MinTrayR(
      document.getElementById('MinTrayR_context'),
      "messenger.watchmessenger",
      function() {
        var menu_NewPopup_id = "";
        try {
          // Seamonkey hack
          let n = document.querySelector('#menu_NewPopup menuitem');
          if (n && !n.id)  {
            n.id = 'newNewMsgCmd';
          }
          menu_NewPopup_id = n.id;
        }
        catch (ex) {
          // no-op
        }

        // Correctly tray-on-close, as Thunderbird 17 owner-draws the window controls in non-aero windows.
        // This will remove the commands from the close button widgets and add an own window handler.
        // Sucks, but there is not really another way.
        (function(self) {
          function MinTrayRTryCloseWindow(event){
            if (self.prefs.getExt("messenger.watchmessenger", true)
              && (self.prefs.getExt('minimizeon', 1) & (1<<1))) {
              self.minimize();
              event.preventDefault();
              event.stopPropagation();
              return false;
            }
            if (event.type == "close") {
              return true;
            }
            // must be in sync with the original command
            return goQuitApplication();
          }
          function MinTrayRTryMinimizeWindow(event) {
            if (self.prefs.getExt("messenger.watchmessenger", true)
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
              // Only available in Thunderbird 17
              return;
            }

            // Remove old command(s)
            button.removeAttribute('command');
            button.removeAttribute('oncommand');

            // Add ourselves
            button.addEventListener('command', newCommand, false);
          }
          ['titlebar-close'].forEach(hijackButton.bind(null, MinTrayRTryCloseWindow));
          ['titlebar-min'].forEach(hijackButton.bind(null, MinTrayRTryMinimizeWindow));
          window.addEventListener("close", MinTrayRTryCloseWindow);
        })(this);

        this.cloneToMenu('MinTrayR_sep-top', [menu_NewPopup_id, "button-getAllNewMsg", "addressBook"], false);
        this.cloneToMenu('MinTrayR_sep-bottom', ['menu_FileQuitItem'], true);
        document
          .getElementById('MinTrayR_' + menu_NewPopup_id)
          .setAttribute(
            'label',
            this.menu.getAttribute('mintrayr_newmessage')
          );
      });
  },
  true
);
