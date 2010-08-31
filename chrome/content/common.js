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
 * Portions created by the Initial Developer are Copyright (C) 2008
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

function MinTrayR(menu, pref) {
	for (let p in MinTrayR.prototype) {
		this[p] = MinTrayR.prototype[p];
	}
	let tp = this;
	addEventListener(
		'unload',
		function() {
			try { tp.restore(); } catch (ex) {/* no op */}
		},
		false
	);
	addEventListener(
		'TrayDblClick',
		function(event) {
			if (event.button == 0 && !!tp.prefs.getExt('dblclickrestore', true)) {
				tp.restore();
			}
		},
		true
	);
	addEventListener(
		'TrayClick',
		function(event) {
			if (event.button == 0 && !tp.prefs.getExt('dblclickrestore', true)) {
				tp.restore();
			}
		},
		true
	);
	if (menu) {
		this.menu = document.getElementById(menu);
		addEventListener(
			'TrayClick',
			function(event) {
				if (event.button == 2 && tp.prefs.getExt('showcontext', true)) {
					tp.showMenu(event.screenX, event.screenY);
				}
			},
			true
		);		
	}
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
	prefs: {},
	
	showMenu: function MinTrayR_showMenu(x, y) {
		this.menu.showPopup(
			document.documentElement,
			x,
			y,
			"context",
			"",
			"bottomleft"
		);
	},
	minimize: function MinTrayR_minimize() {
		this.minimizeWindow(window);
	},
	restore: function MinTrayR_restore() {
		this.restoreWindow(window);
	},
	watch: function MinTrayR_watch() {
		this.watchWindow(window);
	},
	unwatch: function MinTrayR_watch() {
		this.unwatchWindow(window);
	},	
	cloneToMenu: function MinTrayR_cloneToMenu(ref, items, bottom) {
		ref = document.getElementById(ref);
		if (bottom) {
			ref = ref.nextSibling;
		}
		let rv = []
		for each (let id in items) {
			try {
				let node;
				if (typeof id == 'string') {
					node = document.getElementById(id).cloneNode(true);
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
		ref = document.getElementById(ref);
		if (bottom) {
			ref = ref.nextSibling;
		}
		try {
			let node = document.createElement('menuitem');
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
			let n = document.getElementById(arguments[i]);
			if (n && n.hasAttribute('hidden')) {
				n.removeAttribute('hidden');
			}
		}
	},
	hide: function(){
		for (let i = 0; i < arguments.length; ++i) {
			let n = document.getElementById(arguments[i]);
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

Components.utils.import('resource://mintrayr/services.jsm', MinTrayR.prototype);
Components.utils.import('resource://mintrayr/preferences.jsm', MinTrayR.prototype.prefs);
