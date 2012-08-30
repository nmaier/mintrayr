/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/ */

const EXPORTED_SYMBOLS = ['MinTrayR'];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const module = Cu.import;

module("resource://mintrayr/trayservice.jsm");

function MinTrayR(menu, pref, func) {
  if (!menu) {
    throw Error("no menu");
  }
  this.menu = menu;

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
        Cu.reportError("show context/" + event.screenX + "_" + event.screenY);
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

  func.call(this);
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
        Cu.reportError("Failed to clone " + id);
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
      Cu.reportError("Failed to create node");
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
        Cu.reportError(ex);
      }
    }
  },
  minimizeWindow: function(window) {
    TrayService.minimize(window, true);
  },
  restoreWindow: function(window) {
    TrayService.restore(window);
  },
  restoreAll: function() {
    TrayService.restoreAll();
  },
  watchWindow: function(window) {
    TrayService.watchMinimize(window);
  },
  unwatchWindow: function(window) {
    TrayService.unwatchMinimize(window);
  }
};
module('resource://mintrayr/preferences.jsm', MinTrayR.prototype.prefs);
