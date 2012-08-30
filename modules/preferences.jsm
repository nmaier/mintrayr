/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/ */

const EXPORTED_SYMBOLS = [
  'get',
  'getExt',
  'set',
  'setExt',
  'hasUserValue',
  'hasUserValueExt',
  'getChildren',
  'getChildrenExt',
  'reset',
  'resetExt',
  'resetBranch',
  'resetBranchExt',
  'resetAllExt',
  'addObserver',
  'removeObserver'
];

const EXT = 'extensions.mintrayr.';

const Cc = Components.classes;
const Ci = Components.interfaces;
const nsIPrefBranch = Ci.nsIPrefBranch;
const nsIPrefBranch2 = Ci.nsIPrefBranch2;

const PREF_STRING = nsIPrefBranch.PREF_STRING;
const PREF_INT = nsIPrefBranch.PREF_INT;
const PREF_BOOL = nsIPrefBranch.PREF_BOOL;

const SupportsString = Components.Constructor('@mozilla.org/supports-string;1', 'nsISupportsString');

const prefs = Cc['@mozilla.org/preferences-service;1'].getService(Ci.nsIPrefBranch);

function get(key, defaultValue){
  try {
    let rv;
    switch (prefs.getPrefType(key)) {
      case PREF_INT:
        rv = prefs.getIntPref(key);
        break;
      case PREF_BOOL:
        rv = prefs.getBoolPref(key);
        break;
      default:
        rv = getMultiByte(key);
        break;
    }
    if (rv != undefined) {
      return rv;
    }
  }
  catch (ex) {
    // no-op
  }

  return defaultValue;
}

function getExt(key, defaultValue) {
    return get(EXT + key, defaultValue);
}

function set(key, value){
  if (typeof value == 'number' || value instanceof Number) {
    return prefs.setIntPref(key, value);
  }
  if (typeof value == 'boolean' || value instanceof Boolean) {
    return prefs.setBoolPref(key, value);
  }
  return setMultiByte(key, value);
}

function setExt(key, value){
  return set(EXT + key, value);
}

function getMultiByte(key, defaultValue){
  try {
    return prefs.getComplexValue(key, Ci.nsISupportsString).data;
  }
  catch (ex) {
    // no-op
  }
  return defaultValue;
}

function setMultiByte(key, value) {
  let str = new SupportsString();
  str.data = value.toString();
  prefs.setComplexValue(key, Ci.nsISupportsString, str);
}

function hasUserValue(key) {
  try {
    return prefs.prefHasUserValue(key);
  }
  catch (ex) {
    // no-op
  }
  return false;
}

function hasUserValueExt(key) {
  return hasUserValue(EXT + key);
}

function getChildren(key) {
  return prefs.getChildList(key, {});
}

function getChildrenExt(key) {
  return getChildren(EXT + key);
}

function reset(key) {
  try {
    return prefs.clearUserPref(key);
  }
  catch (ex) {
    // no-op
  }
  return false;
}


function resetExt(key) {
  if (key.search(new RegExp('/^' + EXT + '/')) != 0) {
    key = EXT + key;
  }
  return reset(key);
}

function resetBranch(branch) {
  try {
    prefs.resetBranch(branch);
  }
  catch (ex) {
    // BEWARE: not yet implemented in XPCOM 1.8/trunk.
    let children = prefs.getChildList(branch, {});
    for each (let key in children) {
      reset(key);
    }
  }
}

function resetBranchExt(branch) {
  resetBranch('extension.dta.' + branch);
}

function resetAllExt() {
  resetBranchExt('');
}

function addObserver(branch, obj) {
  makeObserver(obj);
  prefs.QueryInterface(nsIPrefBranch2).addObserver(branch, obj, true);
}

function removeObserver(branch, obj) {
  prefs.QueryInterface(nsIPrefBranch2).removeObserver(branch, obj);
}

/**
 * Tiny helper to "convert" given object into a weak observer. Object must still
 * implement .observe()
 *
 * @author Nils
 * @param obj
 *          Object to convert
 */
function makeObserver(obj) {
  try {
    if (
      obj.QueryInterface(Ci.nsISupportsWeakReference)
      && obj.QueryInterface(Ci.nsIObserver)
    ) {
      return;
    }
  }
  catch (ex) {
    // fall-through
  }
  let __QueryInterface = obj.QueryInterface;
  obj.QueryInterface = function(iid) {
    try {
      if (
        iid.equals(Ci.nsISupports)
        || iid.equals(Ci.nsISupportsWeakReference)
        || iid.equals(Ci.nsIWeakReference)
        || iid.equals(Ci.nsIObserver)
      ) {
        return obj;
      }
      if (__QueryInterface) {
        debug("calling original: " + iid);
        return __QueryInterface.call(this, iid);
      }
      throw Components.results.NS_ERROR_NO_INTERFACE;
    }
    catch (ex) {
      debug("requested interface not available: " + iid);
      throw ex;
    }
  };
  // nsiWeakReference
  obj.QueryReferent = function(iid) {
    return obj.QueryInterface(iid);
  };
  // nsiSupportsWeakReference
  obj.GetWeakReference = function() {
    return obj;
  };
}
