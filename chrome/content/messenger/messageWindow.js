/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/ */

var gMinTrayR = {};
addEventListener(
  "load",
  function() {
    removeEventListener("load", arguments.callee, true);

    Components.utils.import("resource://mintrayr/mintrayr.jsm", gMinTrayR);
    gMinTrayr = new gMinTrayR.MinTrayR(
      document.getElementById("MinTrayR_context"),
      'messenger.watchreader',
      function() {
        try {
          // Seamonkey hack
          let n = document.querySelector('#menu_NewPopup menuitem');
          if (n && !n.id)  {
            n.id = 'newNewMsgCmd';
          }
        }
        catch (ex) {
          // no-op
        }
        this.cloneToMenu('MinTrayR_sep-top', ['newNewMsgCmd'], false);
        this.cloneToMenu('MinTrayR_sep-bottom', ['menu_FileQuitItem'], true);
        document
          .getElementById('MinTrayR_newNewMsgCmd')
          .setAttribute(
            'label',
            this.menu.getAttribute('mintrayr_newmessage')
          );
      });
  },
  true
);
