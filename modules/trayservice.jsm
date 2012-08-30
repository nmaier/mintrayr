/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/ */

"use strict";
const EXPORTED_SYMBOLS = ["TrayService"];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;
const module = Cu.import;

module("resource://gre/modules/ctypes.jsm");
module("resource://gre/modules/Services.jsm");
module("resource://gre/modules/XPCOMUtils.jsm");

Services = Object.create(Services);
XPCOMUtils.defineLazyServiceGetter(
  Services,
  "uuid",
  "@mozilla.org/uuid-generator;1",
  "nsIUUIDGenerator"
  );
XPCOMUtils.defineLazyServiceGetter(
  Services,
  "res",
  "@mozilla.org/network/protocol;1?name=resource",
  "nsIResProtocolHandler"
  );
XPCOMUtils.defineLazyServiceGetter(
  Services,
  "appstartup",
  "@mozilla.org/toolkit/app-startup;1",
  "nsIAppStartup"
  );

const _directory = (function() {
  let u = Services.io.newURI(Components.stack.filename, null, null);
  u = Services.io.newURI(Services.res.resolveURI(u), null, null);
  if (u instanceof Ci.nsIFileURL) {
    return u.file.parent.parent;
  }
  throw new Error("not resolved");
})();

const _libraries = {
  "x86-msvc": {m:"tray_x86-msvc.dll",c:ctypes.jschar.ptr},
  "x86_64-msvc": {m:"tray_x86_64-msvc.dll",c:ctypes.jschar.ptr},
  "x86-gcc3": {m:"tray_i686-gcc3.so",c:ctypes.char.ptr},
  "x86_64-gcc3": {m:"tray_x86_64-gcc3.so",c:ctypes.char.ptr}
};
function loadLibrary({m,c}) {
  let resource = _directory.clone();
  resource.append("lib");
  resource.append(m);
  if (!resource.exists()) {
    throw new Error("XPCOMABI Library: " + resource.path)
  }
  return [ctypes.open(resource.path), c];
}

var _icons = [];
var _watchedWindows = [];

const _prefs = Services.prefs.getBranch("extensions.mintrayr.");


const abi_t = ctypes.default_abi;

const handle_t = ctypes.voidptr_t;

const mouseevent_t = ctypes.StructType(
  "mouseevent_t",
  [
  {"button": ctypes.int},
  {"clickCount": ctypes.int},
  {"x": ctypes.long},
  {"y": ctypes.long},
  {"keys": ctypes.int},
  ]
  );

const mouseevent_callback_t = ctypes.FunctionType(
  ctypes.default_abi,
  ctypes.void_t, // retval
  [
  handle_t, // handle
  mouseevent_t.ptr, // event
  ]
  ).ptr;

const minimize_callback_t = ctypes.FunctionType(
  ctypes.default_abi,
  ctypes.void_t, // retval
  [
  handle_t, // handle
  ctypes.int // type
  ]
  ).ptr;

var traylib;
var char_ptr_t;
try {
  // Try to load the library according to XPCOMABI
  [traylib, char_ptr_t] = loadLibrary(_libraries[Services.appinfo.XPCOMABI]);
}
catch (ex) {
  // XPCOMABI yielded wrong results; try alternative libraries
  for (let [,l] in Iterator(_libraries)) {
    try {
      [traylib, char_ptr_t] = loadLibrary(l);
    }
    catch (ex) {
      // no op
    }
  }
  if (!traylib) {
    throw new Error("No loadable library found!");
  }
}

const _Init = traylib.declare(
  "mintrayr_Init",
  abi_t,
  ctypes.void_t // retval
  );
const _Destroy = traylib.declare(
  "mintrayr_Destroy",
  abi_t,
  ctypes.void_t // retval
  );
const _GetBaseWindowHandle = traylib.declare(
  "mintrayr_GetBaseWindow",
  abi_t,
  handle_t, // retval handle
  char_ptr_t // title
  );
const _SetWatchMode = traylib.declare(
  "mintrayr_SetWatchMode",
  abi_t,
  ctypes.void_t, // retval handle
  ctypes.int // mode
);
const _MinimizeWindow = traylib.declare(
  "mintrayr_MinimizeWindow",
  abi_t,
  ctypes.void_t, // retval BOOL
  handle_t // handle
  );
const _RestoreWindow = traylib.declare(
  "mintrayr_RestoreWindow",
  abi_t,
  ctypes.void_t, // retval BOOL
  handle_t // handle
  );
const _CreateIcon = traylib.declare(
  "mintrayr_CreateIcon",
  abi_t,
  ctypes.int, // retval BOOL
  handle_t, // handle
  mouseevent_callback_t // callback
  );
const _DestroyIcon = traylib.declare(
  "mintrayr_DestroyIcon",
  abi_t,
  ctypes.int, // retval BOOL
  handle_t // handle
  );
const _WatchWindow = traylib.declare(
  "mintrayr_WatchWindow",
  abi_t,
  ctypes.int, // retval BOOL
  handle_t, // handle
  minimize_callback_t // callback
  );
const _UnwatchWindow = traylib.declare(
  "mintrayr_UnwatchWindow",
  abi_t,
  ctypes.int, // retval BOOL
  handle_t // handle
  );

function GetBaseWindowHandle(window) {
  let baseWindow = window
    .QueryInterface(Ci.nsIInterfaceRequestor)
    .getInterface(Ci.nsIWebNavigation)
    .QueryInterface(Ci.nsIBaseWindow);

  // Tag the base window
  let oldTitle = baseWindow.title;
  baseWindow.title = Services.uuid.generateUUID().toString();

  let rv;
  try {
    // Search the window by the new title
    rv = _GetBaseWindowHandle(baseWindow.title);
    if (rv.isNull()) {
      throw new Error("Window not found!");
    }
  }
  finally {
    // Restore
    baseWindow.title = oldTitle;
  }
  return rv;
}

function ptrcmp(p1, p2) p1.toString() == p2.toString()

const mouseevent_callback = mouseevent_callback_t(function mouseevent_callback(handle, event) {
  try {
    event = event.contents;
    for (let [,w] in Iterator(_icons)) {
      if (!ptrcmp(w.handle, handle)) {
        continue;
      }
      let document = w.window.document;
      let e = document.createEvent("MouseEvents");
      let et = "TrayClick";
      if (event.clickCount == 2) {
        et = "TrayDblClick";
      }
      else if (event.clickCount > 2) {
        et = "TrayTriClick";
      }
      e.initMouseEvent(
        et,
        true,
        true,
        w.window,
        0,
        event.x,
        event.y,
        0,
        0,
        (event.keys & (1<<0)) != 0,
        (event.keys & (1<<1)) != 0,
        (event.keys & (1<<2)) != 0,
        (event.keys & (1<<3)) != 0,
        event.button,
        document
        );
      document.dispatchEvent(e);
      return;
    }
    throw new Error("Window for mouse event not found!" + _icons.toSource());
  }
  catch (ex) {
    Cu.reportError(ex);
  }
});

const minimize_callback = minimize_callback_t(function minimize_callback(handle, type) {
  try {
    for (let [,w] in Iterator(_watchedWindows)) {
      if (ptrcmp(w.handle, handle)) {
        if (!type) {
          TrayService.minimize(w.window, true);
        }
        else {
          TrayService.restore(w.window);
        }
        return 1;
      }
    }
  }
  catch (ex) {
    // no op
  }
  return 0;
});

function WatchedWindow(window) {
  this._handle = GetBaseWindowHandle(window);
  try {
    this._window = window;
    _WatchWindow(this._handle, minimize_callback);
  }
  catch (ex) {
    delete this._handle;
    delete this._window;
    throw ex;
  }
}
WatchedWindow.prototype = {
  get window() this._window,
  get handle() this._handle,
  destroy: function() {
    try {
      _UnwatchWindow(this._handle);
    }
    finally {
      // drop the references;
      delete this._handle;
      delete this._window;
    }
  },
  toString: function() "[WatchedWindow @" + this._handle + "]"
};

function TrayIcon(window, aCloseOnRestore) {
  this._handle = GetBaseWindowHandle(window);
  try {
    _CreateIcon(this._handle, mouseevent_callback);
  }
  catch (ex) {
    delete this._handle;
    throw ex;
  }

  this._window = window;
  this.closeOnRestore = aCloseOnRestore;
  this.window.addEventListener("unload", this, false);
}
TrayIcon.prototype = {
  _closed: false,
  _minimized: false,
  get handle() this._handle,
  get window() this._window,
  get isMinimized() this._minimized,
  minimize: function() {
    if (this._closed) {
      throw new Error("Icon already closed");
    }
    if (this._minimized) {
      return;
    }
    _MinimizeWindow(this._handle);
    this._minimized = true;
  },
  restore: function() {
    if (this._closed) {
      throw new Error("Icon already closed");
    }
    if (!this._minimized) {
      return;
    }
    if (this.closeOnRestore) {
      this.close();
    }
    else {
      _RestoreWindow(this._handle);
    }
    this._minimized = false;
  },
  close: function() {
    if (this._closed){
      return;
    }
    this._closed = true;

    _DestroyIcon(this._handle);
    this._window.removeEventListener("unload", this, false);
    TrayService._closeIcon(this);

    delete this._handle;
    delete this._window;
  },
  handleEvent: function(event) {
    this.close();
  },
  toString: function() "[Icon @" + this._handle + "]"
};

const TrayService = {
  createIcon: function(window, aCloseOnRestore) {
    for (let [,icon] in Iterator(_icons)) {
      if (icon.window === window) {
        return icon;
      }
    }
    let icon = new TrayIcon(window, aCloseOnRestore);
    _icons.push(icon);
    return icon;
  },
  restoreAll: function() {
    for (let [,icon] in Iterator(_icons)) {
      icon.restore();
    }
    _icons.length = 0;
  },
  watchMinimize: function(window) {
    if (this.isWatchedWindow(window)) {
      return;
    }
    let ww = new WatchedWindow(window);
    _watchedWindows.push(ww);
  },
  unwatchMinimize: function(window) {
    for (let [i,w] in Iterator(_watchedWindows)) {
      if (w.window === window) {
        try {
          w.destroy();
        }
        finally {
          _watchedWindows.splice(i, 1);
        }
        return;
      }
    }
  },
  isWatchedWindow: function(window) {
    for (let [i,w] in Iterator(_watchedWindows)) {
      if (w.window === window) {
        return true;
      }
    }
    return false;
  },
  minimize: function(window, aCloseOnRestore) this.createIcon(window, aCloseOnRestore).minimize(),
  restore: function(window) {
    for (let [,icon] in Iterator(_icons)) {
      if (icon.window === window) {
        icon.restore();
        return;
      }
    }
    throw new Error("Invalid window to be restored specified");
  },
  _closeIcon: function(icon) {
    let idx = _icons.indexOf(icon);
    if (idx >= 0) {
      _icons.splice(idx, 1);
    }
  },
  _replaySessionStoreByAbusingCuImport: function you_didnt_see_this__im_serious__do_not_attempt_at_home() {
    try {
      const {SessionStoreInternal: ssi} = module("resource:///modules/sessionstore/SessionStore.jsm", {});
      // Force re-collect all windows, so that the coords of previously minimized windows aren't all messed up
      // See GH-84, GH-89
      ssi.onQuitApplicationRequested();
    }
    catch (ex) {}
  },
  _shutdown: function() {
    for (let [,icon] in Iterator(_icons)) {
      icon.close();
    }
    _icons.length = 0;

    for (let [,w] in Iterator(_watchedWindows)) {
      w.destroy();
    }
    _watchedWindows.length = 0;
    this._replaySessionStoreByAbusingCuImport();
  }
};

const Observer = {
  register: function() {
    Services.obs.addObserver(Observer, "quit-application", false);
    Services.obs.addObserver(Observer, "quit-application-granted", false);
    Services.prefs.addObserver("extensions.mintrayr.minimizeon", Observer, false);
    this.setWatchMode();
  },
  unregister: function() {
    Services.obs.removeObserver(Observer, "quit-application-granted");
    Services.obs.removeObserver(Observer, "quit-application");
    Services.prefs.removeObserver("extensions.mintrayr.minimizeon", Observer);
  },
  setWatchMode: function() {
    _SetWatchMode(Services.prefs.getIntPref("extensions.mintrayr.minimizeon"));
  },
  observe: function(s, topic, data) {
    if (topic == "quit-application-granted" || topic == "quit-application") {
      this.unregister();
      Services.appstartup.enterLastWindowClosingSurvivalArea();
      try {
        TrayService._shutdown();
      }
      finally {
        Services.appstartup.exitLastWindowClosingSurvivalArea();
      }
    }
    else {
      this.setWatchMode();
    }
  }
};
Observer.register();

_Init();
