"use strict";

const EXPORTED_SYMBOLS = ["init", "createIcon", "isWatched", "watchMinimize", "unwatchMinimize"];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;
const module = Cu.import;

module("resource://gre/modules/ctypes.jsm");
module("resource://gre/modules/Services.jsm");
module("resource://gre/modules/XPCOMUtils.jsm");

const Services2 = {};
XPCOMUtils.defineLazyServiceGetter(
  Services2,
  "uuid",
  "@mozilla.org/uuid-generator;1",
  "nsIUUIDGenerator"
  );
XPCOMUtils.defineLazyServiceGetter(
  Services2,
  "res",
  "@mozilla.org/network/protocol;1?name=resource",
  "nsIResProtocolHandler"
  );
XPCOMUtils.defineLazyServiceGetter(
  Services2,
  "appstartup",
  "@mozilla.org/toolkit/app-startup;1",
  "nsIAppStartup"
  );

const directory = (function() {
  let u = Services.io.newURI(Components.stack.filename, null, null);
  u = Services.io.newURI(Services2.res.resolveURI(u), null, null);
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

var _watchedWindows = [];
var _icons = [];

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


function loadLibrary({m,c}) {
  let resource = directory.clone();
  resource.append("lib");
  resource.append(m);
  if (!resource.exists()) {
    throw new Error("XPCOMABI Library: " + resource.path)
  }
  return [ctypes.open(resource.path), c];
}

var traylib;
var char_ptr_t;
try {
  [traylib, char_ptr_t] = loadLibrary(_libraries[Services.appinfo.XPCOMABI]);
}
catch (ex) {
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

const abi_t = ctypes.default_abi;

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
  baseWindow.title = Services2.uuid.generateUUID().toString();

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
    for (let [,w] in Iterator(_watchedWindows)) {
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
        return w.callback(w.window, type);
      }
    }
  }
  catch (ex) {
    Cu.reportError(ex);
  }
  return 0;
});

function WatchedWindow(window, callback) {
  this._handle = GetBaseWindowHandle(window);
  try {
    this._window = window;
    this._callback = callback;
    _WatchWindow(this._handle, minimize_callback);
  }
  catch (ex) {
    delete this._handle;
    delete this._window;
    delete this._callback;
    throw ex;
  }
}
WatchedWindow.prototype = {
  get window() this._window,
  get handle() this._handle,
  get callback() this._callback,
  destroy: function() {
    try {
      _UnwatchWindow(this._handle);
    }
    finally {
      // drop the references;
      delete this._handle;
      delete this._window;
      delete this._callback;
    }
  },
  toString: function() "[WatchedWindow @" + this._handle + "]"
};

function Icon(window) {
  this._handle = GetBaseWindowHandle(window);
  try {
    this._window = window;
    _CreateIcon(this._handle, mouseevent_callback);
  }
  catch (ex) {
    delete this._handle;
    delete this._window;
    throw ex;
  }
}
Icon.prototype = {
  get window() this._window,
  get handle() this._handle,
  minimize: function() {
    _MinimizeWindow(this._handle);
  },
  restore: function(){
    _RestoreWindow(this._handle);
  },
  destroy: function() {
    try {
      _DestroyIcon(this._handle);
    }
    finally {
      // drop the references;
      for (let [i,icon] in Iterator(_icons)) {
        if (icon.window == this.window) {
          _icons.splice(i, 1);
          break;
        }
      }
      delete this._handle;
      delete this._window;
    }
  },
  toString: function() "[Icon @" + this._handle + "]"
};

function createIcon(window) {
  for (let [,icon] in Iterator(_icons)) {
    if (icon.window == window) {
      return icon;
    }
  }
  let icon = new Icon(window);
  _icons.push(icon);
  return icon;
}

function isWatched(window) {
  for (let [i,w] in Iterator(_watchedWindows)) {
    if (w.window === window) {
      return true;
    }
  }
  return false;
}

function watchMinimize(window, callback) {
  if (isWatched(window)) {
    return;
  }
  let ww = new WatchedWindow(window, callback);
  _watchedWindows.push(ww);
}
function unwatchMinimize(window) {
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
}

function unwatchAll(){
  Services2.appstartup.enterLastWindowClosingSurvivalArea();
  try {
    for (let [,w] in _watchedWindows) {
      w.destroy();
    }
    _watchedWindows.length = 0;
  }
  finally {
    appstartup.exitLastWindowClosingSurvivalArea();
  }
}

const Observer = {
  register: function() {
    Services.obs.addObserver(Observer, "quit-application", false);
    Services.prefs.addObserver("extensions.mintrayr.minimizeon", Observer, false);
    this.setWatchMode();
  },
  unregister: function() {
    Services.obs.removeObserver(Observer, "quit-application");
    Services.prefs.removeObserver("extensions.mintrayr.minimizeon", Observer);
  },
  setWatchMode: function() {
    _SetWatchMode(Services.prefs.getIntPref("extensions.mintrayr.minimizeon"));
  },
  observe: function(s, topic, data) {
    if (topic == "quit-application") {
      this.unregister();
      unwatchAll();
    }
    else {
      this.setWatchMode();
    }
  }
};
Observer.register();

_Init();
