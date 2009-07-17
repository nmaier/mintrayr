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

const MinTrayR_CallbackTimer = Components.Constructor('@mozilla.org/timer;1', 'nsITimer', 'initWithCallback');

function MinTrayRSongbird() {
	MinTrayR.call(this, 'MinTrayR_context', 'songbird.watchplayer');
	var nodes = [
	  this.cloneToMenu('MinTrayR_sep-top', ['menuitem_control_play', 'menuitem_control_next', 'menuitem_control_prev'], false),
		this.cloneToMenu('MinTrayR_sep-middle', ['menuitem_control_repx', 'menuitem_control_repa', 'menuitem_control_rep1'], false),
		this.cloneToMenu('MinTrayR_sep-bottom', ['menu_FileQuitItem'], true)
	];
	
	addEventListener(
		'TrayMinimize',
		function(evt) gMinTrayR && (gMinTrayR._mayFloat = false),
		true
	);
	addEventListener(
		'TrayRestore',
		function(evt) gMinTrayR && (gMinTrayR._mayFloat = true),
		true
	);
	addEventListener(
		'TrayClick',
		function(evt) {
			if (evt.button == 1 && gMinTrayR && gMinTrayR.prefs.getExt('songbird.middleclickplays')) {
				doMenu('menuitem_control_play');
			}
		},
		true
	);
	
	
	for each (let n in nodes) {
		for each (let e in n) {
			e.setAttribute('oncommand', 'doMenu(event.target.getAttribute("mintrayr_origid"))');
		}
	}
	try {
		if (!LastFm || !LastFm._service || !LastFm._service.loveBan || !LastFm._strings) {
			throw new Error("LastFm not installed!");
		}
		document.getElementById('MinTrayR_lastFM-love').setAttribute('label', LastFm._strings.getString('lastfm.faceplate.love.tooltip'));
		document.getElementById('MinTrayR_lastFM-ban').setAttribute('label', LastFm._strings.getString('lastfm.faceplate.ban.tooltip'));
		this.show('MinTrayR_lastFM-sep', 'MinTrayR_lastFM-love', 'MinTrayR_lastFM-ban');
	}
	catch (ex) {
		this.hide('MinTrayR_lastFM-sep', 'MinTrayR_lastFM-love', 'MinTrayR_lastFM-ban');
	}
	this._sp = {};
	Components.utils.import('resource://app/jsmodules/sbProperties.jsm', this._sp);
	this._sp = this._sp.SBProperties;
	this._mm.addListener(this);
	
	this._titleTimer = new MinTrayR_CallbackTimer(this, 300, 0x1);
}
MinTrayRSongbird.prototype = {
	_sp: null,
	_mm: Components.classes["@songbirdnest.com/Songbird/Mediacore/Manager;1"]
		.getService(Components.interfaces.sbIMediacoreManager),
	evt: Components.interfaces.sbIMediacoreEvent,
	
	_mayFloat: true,
	
	_title: '',
	_floatTitle: '',
	get windowTitle() {
		if (!this._title) {
			return document.title;
		}
	 	return (this._mayFloat ? this._floatTitle : this._title) + ' - Songbird';
	},
	
	unload: function(evt) {
		this.log('unload');
		if (this._titleTimer) {
			this._titleTimer.cancel();
			delete this._titleTimer;
		}
		return true;
	},
	onMediacoreEvent: function(evt) {
		if (evt.type == this.evt.TRACK_CHANGE) {
			let item = evt.data;
			let sp = this._sp;
			let rs = this.prefs
				.getExt('songbird.titlestring', '%trackName% / %artistName%')
				.replace(
					/%[\w\d]+?%/g,
					function(data) {
						let rv = '';
						try {
							rv = item.getProperty(sp[data.substr(1, data.length - 2)]);
							rv = rv || '';
						}
						catch (ex) {
							// no-op
						}
						return rv;
					}
				);
			this._title = rs;
			this._floatTitle = rs + "   ";
			document.title = this.windowTitle;
		}
	},
	
	notify: function(subject, topic, data) {
		if (!this._title) {
			return;
		}
		if (this.prefs.getExt('songbird.titlefloating', false)) {
			this._floatTitle = this._floatTitle.substr(1) + this._floatTitle[0];
		}
		document.title = this.windowTitle;
	},
		
	loveBan_LastFm: function(loved) {
		// unlove/unban
		if (LastFm._service.loveTrack && LastFm._service.love == loved) {
			LastFm._service.loveBan(null, false);
		}
		else {
			LastFm._service.loveBan(gMM.sequencer.currentItem.guid, loved);
		}
	},
	closeClicked: function() {
		if ((this.prefs.getExt('minimizeon', 1) & (1 << 1)) != 0) {
			this.minimize();
			return true;
		}
		return false;
	}
};

var gMinTrayR;
addEventListener(
	'load',
	function() gMinTrayR = new MinTrayRSongbird(),
	true
);
addEventListener(
	'unload',
	function(evt) gMinTrayR && gMinTrayR.unload(evt),
	false
);

/* Songbird workaround
 * SB does not issue standard OS messages for the close button,
 * but instead implements it's own bindings and calls the close
 * function directly.
 */
if ('quitApp' in this) {
	quitApp = (function() {
		let _p = quitApp;
		return function() {
			try {
				// examine the stack; if we're called from the sys buttons binding then 
				// see if we should minimize instead
				let stack = (new Error()).stack.toString();
				if (
					stack.indexOf('chrome://songbird/content/bindings/sysControls.xml') != -1
					&& gMinTrayR.closeClicked() 
				) {
					return null;
				}
			}
			catch (ex) {
				gMinTrayR.log(ex);
			}
			return _p.apply(this, arguments);			
		}
	})();
}