/*
Creative Commons License: Attribution-No Derivative Works 3.0 Unported
http://creativecommons.org/licenses/by-nd/3.0/
(c)2009 Michael Koch
*/

var objTelifyPrefs = {

PREF_BLACKLIST: "blacklist",
PREF_HIGHLIGHT: "highlight",
PREF_EXCLUDE: "exclude",
PREF_DEBUG: "debug",
PREF_ACTIVE: "active",
PREF_STATUSICON: "statusicon",
PREF_HREFTYPE: "linktype",
PREF_COLSORTCC: "colsortcc",
PREF_NUMHISTORY: "num_history",
PREF_IDD_PREFIX: "idd_prefix",
PREF_DONT_ESCAPE_PLUS: "dont_escape_plus",
PREF_DIAL_CC_DIRECT: "dial_cc_direct",

NUM_CUSTOM_PARAMS: 3,

PREF_CUSTOM_URL: "custom_url",
PREF_CUSTOM_TMPL: "custom_tmpl",
PREF_CUSTOM_PARAM: "custom_param",
PREF_CUSTOM_OPENTYPE: "custom_opentype",

maxHistory: 10,

telPrefs: null,
telStrings: null,

blacklist: null,
excludedHosts: null,
highlight: null,
excludedTags: null,
hrefType: null,
numHistory: null,
idd_prefix: null,
fStatusIcon: null,
fActive: null,
fDebug: null,
fDontEscapePlus: null,
fDialCCDirect: null,

custom_url: null,
custom_tmpl: null,
custom_param: [],
custom_opentype: null,

HREFTYPE_CUSTOM: 9,

protoList: new Array("tel", "callto", "skype", "sip"),


showConfigDialog: function()
{
	while (true) {
		window.openDialog("chrome://telify/content/config.xul", "dlgTelifyConfig", "centerscreen,chrome,modal").focus;
		if (this.hrefType == this.HREFTYPE_CUSTOM && this.custom_url.indexOf("$0") < 0) {
			var result = objTelifyUtil.showMessageBox("", objTelifyLocale.msgNumberTemplateMissing(),
				 objTelifyUtil.MB_OK|objTelifyUtil.MB_CANCEL|objTelifyUtil.MB_ICON_WARNING);
			if (result == false) continue;
		}
		break;
	}
},


getPrefObj: function()
{
	var obj = Components.classes["@mozilla.org/preferences-service;1"];
	obj = obj.getService(Components.interfaces.nsIPrefService);
	obj = obj.getBranch("telify.settings.");
	obj.QueryInterface(Components.interfaces.nsIPrefBranch2);
	return obj;
},


getCharPref: function(name)
{
	try {
		return this.telPrefs.getCharPref(name);
	} catch (e) {
		alert(e);
		return "";
	}
},


getIntPref: function(name)
{
	try {
		return this.telPrefs.getIntPref(name);
	} catch (e) {
		return 0;
	}
},


getBoolPref: function(name)
{
	try {
		return this.telPrefs.getBoolPref(name);
	} catch (e) {
		return false;
	}
},


getPrefs: function()
{
	this.blacklist = this.telPrefs.getCharPref(this.PREF_BLACKLIST);
	if (this.blacklist.length > 0) {
		this.excludedHosts = this.blacklist.toLowerCase().split(",");
	} else {
		this.excludedHosts = new Array();
	}
	this.highlight = this.telPrefs.getIntPref(this.PREF_HIGHLIGHT);
	this.highlight = objTelifyUtil.trimInt(this.highlight, 0, 100);
	this.numHistory = this.telPrefs.getIntPref(this.PREF_NUMHISTORY);
	this.numHistory = objTelifyUtil.trimInt(this.numHistory, 1, 10);
	this.idd_prefix = this.telPrefs.getCharPref(this.PREF_IDD_PREFIX);
	var exclude = this.telPrefs.getCharPref(this.PREF_EXCLUDE);
	this.excludedTags = exclude.toLowerCase().split(",");
	this.hrefType = this.telPrefs.getIntPref(this.PREF_HREFTYPE);
	if ((this.hrefType < 0 || this.hrefType >= this.protoList.length) && this.hrefType != this.HREFTYPE_CUSTOM) this.hrefType = 0;
	this.fStatusIcon = this.telPrefs.getBoolPref(this.PREF_STATUSICON);
	var status = document.getElementById("idTelify_status");
	if (status) status.setAttribute("collapsed", !this.fStatusIcon);
	this.fDebug = this.telPrefs.getBoolPref(this.PREF_DEBUG);
	this.fActive = this.telPrefs.getBoolPref(this.PREF_ACTIVE);
	this.fDontEscapePlus = this.telPrefs.getBoolPref(this.PREF_DONT_ESCAPE_PLUS);
	this.fDialCCDirect = this.telPrefs.getBoolPref(this.PREF_DIAL_CC_DIRECT);
	// custom url
	this.custom_url = this.getCharPref(this.PREF_CUSTOM_URL);
	this.custom_tmpl = this.getIntPref(this.PREF_CUSTOM_TMPL);
	for (var i=1; i<this.NUM_CUSTOM_PARAMS+1; i++) {
		this.custom_param[i] = this.getCharPref(this.PREF_CUSTOM_PARAM+i);
	}
	this.custom_opentype = this.getIntPref(this.PREF_CUSTOM_OPENTYPE);
},


prefObserver: {
	observe: function(subject, topic, data) {
		if (topic != "nsPref:changed") return;
		objTelifyPrefs.getPrefs();
	}
},


initTelifyPrefs: function()
{
	objTelifyPrefs.telPrefs = objTelifyPrefs.getPrefObj();
	objTelifyPrefs.telPrefs.addObserver("", objTelifyPrefs.prefObserver, false);
	objTelifyPrefs.telStrings = document.getElementById("idTelifyStringBundle");
	objTelifyPrefs.getPrefs();
}

};


