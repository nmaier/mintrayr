/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/ */

var gMinTrayR = {};
addEventListener(
  "load",
  function() {
    removeEventListener("load", arguments.callee, true);

    Components.utils.import("resource://mintrayr/mintrayr.jsm", gMinTrayR);
    gMinTrayR = new gMinTrayR.MinTrayR(
      document.getElementById("MinTrayR_context"),
      'messenger.watchcomposer',
      function() {
        this.cloneToMenu(
          'MinTrayR_sep-top',
          [document.getElementById('menu_New').getElementsByTagName('menuitem')[0]],
          false
        )[0].setAttribute(
          'label',
          this.menu.getAttribute('mintrayr_newmessage')
        );
        document.getElementById('MinTrayR_sep-bottom').hidden = true;
      });
  },
  true
);
