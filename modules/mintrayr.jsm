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
 * Portions created by the Initial Developer are Copyright (C) 2008,2010
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

const EXPORTED_SYMBOLS = ['MinTrayR'];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const module = Cu.import;

function MinTrayR(menu, pref) {
  if (!menu) {
    throw Error("no menu");
  }
  this.menu = menu;

  // Add our prototype
  this.__proto__ = MinTrayR.prototype;

  this.document = menu.ownerDocument;
  this.window = this.document.defaultView;

  let tp = this;
  this.window.addEventListener(
    'TrayDblClick',
    function(event) {
      if (event.button == 0 && !!tp.prefs.getExt('dblclickrestore', true)) {
        tp.restore();
      }
    },
    true
  );
  this.window.addEventListener(
    'TrayClick',
    function(event) {
      if (event.button == 0 && !tp.prefs.getExt('dblclickrestore', true)) {
        tp.restore();
      }
    },
    true
  );
  this.window.addEventListener(
    'TrayClick',
    function(event) {
      if (event.button == 2 && tp.prefs.getExt('showcontext', true)) {
        tp.showMenu(event.screenX, event.screenY);
      }
    },
    true
  );
  if (typeof pref == 'boolean' && pref) {
    this.watch();
  }
  else if (pref) {
    this._watchPref = pref;
    this.prefs.addObserver('extensions.mintrayr.' + this._watchPref, this);
    this.observe(null, null, pref);
  }
}

MinTrayR.prototype = {
  _watchPref: null,
  _watched: false,
  prefs: {},

  showMenu: function MinTrayR_showMenu(x, y) {
    this.menu.showPopup(
      this.document.documentElement,
      x,
      y,
      "context",
      "",
      "bottomleft"
    );
  },
  minimize: function MinTrayR_minimize() {
    this.minimizeWindow(this.window);
  },
  restore: function MinTrayR_restore() {
    this.restoreWindow(this.window);
  },
  get isWatched() this._watched,
  watch: function MinTrayR_watch() {
    if (!this._watched) {
      this.watchWindow(this.window);
      this._watched = true;
    }
  },
  unwatch: function MinTrayR_watch() {
    if (this._watched) {
      this.unwatchWindow(this.window);
      this._watched = false;
    }
  },
  cloneToMenu: function MinTrayR_cloneToMenu(ref, items, bottom) {
    ref = this.document.getElementById(ref);
    if (bottom) {
      ref = ref.nextSibling;
    }
    let rv = []
    for each (let id in items) {
      try {
        let node;
        if (typeof id == 'string') {
          node = this.document.getElementById(id).cloneNode(true);
        }
        else {
          node = id;
        }

        node.setAttribute('mintrayr_origid', node.id);
        node.setAttribute('id', 'MinTrayR_' + node.id);
        if (node.hasAttribute('hidden')) {
          node.removeAttribute('hidden');
        }
        this.menu.insertBefore(node, ref);
        rv.push(node);
      }
      catch (ex) {
        this.log("Failed to clone " + id);
      }
    }
    return rv;
  },
  addToMenu: function(ref, attrs) {
    ref = this.document.getElementById(ref);
    if (bottom) {
      ref = ref.nextSibling;
    }
    try {
      let node = this.document.createElement('menuitem');
      for (let attr in attrs) {
        node.setAttribute(attr, attrs[attr]);
      }
      this.menu.insertBefore(node, ref);
      return [node];
    }
    catch (ex) {
      this.log("Failed to create node");
    }
    return [];
  },
  show: function(){
    for (let i = 0; i < arguments.length; ++i) {
      let n = this.document.getElementById(arguments[i]);
      if (n && n.hasAttribute('hidden')) {
        n.removeAttribute('hidden');
      }
    }
  },
  hide: function(){
    for (let i = 0; i < arguments.length; ++i) {
      let n = this.document.getElementById(arguments[i]);
      if (n) {
        n.setAttribute('hidden', true);
      }
    }
  },
  observe: function(subject, topic, data) {
    if (data.indexOf(this._watchPref) != -1) {
      try {
        if (this.prefs.getExt(this._watchPref)) {
          this.watch();
        }
        else {
          this.unwatch();
        }
      }
      catch (ex) {
        this.log(ex);
      }
    }
  }
};
module('resource://mintrayr/services.jsm', MinTrayR.prototype);
module('resource://mintrayr/preferences.jsm', MinTrayR.prototype.prefs);
