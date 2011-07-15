const EXPORTED_SYMBOLS = ["TrayService"];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;
const module = Cu.import;

module("resource://gre/modules/Services.jsm");
module("resource://gre/modules/XPCOMUtils.jsm");

const Platform = {};

const _icons = [];
const _prefs = Services.prefs.getBranch("extensions.mintrayr.");

function TrayIcon(window, aCloseOnRestore) {
	this._window = window;
	this.closeOnRestore = aCloseOnRestore;
	this._icon = Platform.createIcon(window);
	this.window.addEventListener("unload", this, false);
}
TrayIcon.prototype = {
	_closed: false,
	_minimized: false,
	get window() this._window,
	get isMinimized() this._minimized,
	minimize: function() {
		if (this._closed) {
			throw new Error("Icon already closed");
		}
		if (this._minimized) {
			return;
		}
		this._icon.minimize();
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
			this._icon.restore();
		}
		this._minimized = false;
	},
	close: function() {
		if (this._closed){
			return;
		}
		this._closed = true;

		this._icon.destroy();
		this._window.removeEventListener("unload", this, false);
		TrayService._closeIcon(this);

		delete this._icon;
		delete this._window;
	},
	handleEvent: function(event) {
		this.close();
	}
};

function doMinimizeWindow(window) {
	try {
		TrayService.minimize(window, true);
	}
	catch (ex) {
		Cu.reportError(ex);
	}
	return 0;
}

const TrayService = {
	_initialized: false,
	init: function(callback) {
		module("resource://mintrayr/platform.jsm", Platform);
		Platform.init((function() {
			this._initialized = true;
			callback();
		}).bind(this));
	},
	createIcon: function(window, aCloseOnRestore) {
		for (let [,icon] in Iterator(_icons)) {
			if (icon.window == window) {
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
		if (Platform.isWatched(window)) {
			return;
		}
		Platform.watchMinimize(window, doMinimizeWindow);
	},
	unwatchMinimize: function(window) {
		Platform.unwatchMinimize(window);
	},
	isWatchedWindow: function(window) Platform.isWatched(window),
	minimize: function(window, aCloseOnRestore) this.createIcon(window, aCloseOnRestore).minimize(),
	restore: function(window) {
		for (let [,icon] in Iterator(_icons)) {
			if (icon.window == window) {
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
	_shutdown: function() {
		for (let [,icon] in Iterator(_icons)) {
			icon.close();
		}
		_icons.length = 0;
	}

}

Services.obs.addObserver({
	QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),
	observe: function() {
		Services.obs.removeObserver(this, "quit-application");
		TrayService._shutdown();
	}
}, "quit-application", false);
