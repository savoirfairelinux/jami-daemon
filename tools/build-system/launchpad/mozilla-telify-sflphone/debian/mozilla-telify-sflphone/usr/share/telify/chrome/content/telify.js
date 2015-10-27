/*
Creative Commons License: Attribution-No Derivative Works 3.0 Unported
http://creativecommons.org/licenses/by-nd/3.0/
(c)2009 Michael Koch
*/

var objTelify = {

digits_min: 7,
digits_max: 16,

hilite_color: new Array(0,0,255),
hilite_bgcolor: new Array(255,255,0),

// special chars
sc_nbsp: String.fromCharCode(0xa0),

// chars which look like dashes
token_dash:
	String.fromCharCode(0x2013) +
	String.fromCharCode(0x2014) +
	String.fromCharCode(0x2212),

exclPatternList: [
	/^\d{2}\.\d{2} ?(-|–) ?\d{2}\.\d{2}$/,	// time range e.g. 08.00 - 17.00
	/^\d{2}\/\d{2}\/\d{2}$/,	// date e.g. 09/03/09
	/^\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}$/,	// ip address
	/^[0-3]?[0-9]\.[0-3]?[0-9]\.(19|20)\d{2} - \d{2}\.\d{2}$/,	// date and time e.g. 09.03.2009 - 17.59
	/^[0-3]?[0-9][\/\.-][0-3]?[0-9][\/\.-](19|20)\d{2}$/,	// date e.g. 09/03/2009, 09.03.2009, 09-03-2009
	/^[0-3]?[0-9][\/\.-][0-3]?[0-9][\/\.-]\d{2} ?(-|–) ?[0-3]?[0-9][\/\.-][0-3]?[0-9][\/\.-]\d{2}$/,	// date range short
	/^[0-3]?[0-9][\/\.-][0-3]?[0-9][\/\.-] ?(-|–) ?[0-3]?[0-9][\/\.-][0-3]?[0-9][\/\.-](19|20)\d{2}$/,	// date range medium
	/^[0-3]?[0-9][\/\.-][0-3]?[0-9][\/\.-](19|20)\d{2} ?(-|–) ?[0-3]?[0-9][\/\.-][0-3]?[0-9][\/\.-](19|20)\d{2}$/,	// date range long
	/^0\.\d+$/, // e.g. 0.12345678
],

inclLocalList: [
	[/^[1-9]\d{2}[\.-]\d{3}[\.-]\d{4}$/, "+1"],	// US
],

token_trigger: "+(0123456789",
token_part: " -/()[].\r\n"
	+ String.fromCharCode(0xa0) // sc_nbsp
	+ String.fromCharCode(0x2013) + String.fromCharCode(0x2014) +	String.fromCharCode(0x2212), // token_dash
token_start: "+(0",
token_sep: " -/(.",
token_disallowed_post: ":-",
token_disallowed_prev: "-,.",

dialHistory: new Array(objTelifyPrefs.maxHistory),


getDialHistory: function()
{
	for (var i=0; i<objTelifyPrefs.maxHistory; i++) {
		try {
			this.dialHistory[i] = objTelifyPrefs.telPrefs.getCharPref("history"+i);
		} catch (e) {
			this.dialHistory[i] = "";
		}
	}
},


saveDialHistory: function()
{
	for (var i=0; i<objTelifyPrefs.maxHistory; i++) {
		if (this.dialHistory[i] == null) this.dialHistory[i] = "";
		objTelifyPrefs.telPrefs.setCharPref("history"+i, this.dialHistory[i]);
	}
},


updateDialHistory: function(prefix)
{
	//logmsg("updateDialHistory("+prefix+")");
	var newList = new Array(objTelifyPrefs.maxHistory);
	newList[0] = prefix;
	for (var i=0, j=1; i<objTelifyPrefs.maxHistory && j<objTelifyPrefs.maxHistory; i++) {
		if (this.dialHistory[i] == null || this.dialHistory[i] == "" || this.dialHistory[i] == prefix) continue;
		newList[j++] = this.dialHistory[i];
	}
	this.dialHistory = newList;
	this.saveDialHistory();
},


setStatus: function()
{
	var statusicon = document.getElementById("idTelify_statusicon");
	if (objTelifyPrefs.fActive) {
		statusicon.setAttribute("src", "chrome://telify/content/icon18_active.png");
		var text = objTelifyPrefs.telStrings.getString("telify_active");
		statusicon.setAttribute("tooltiptext", text);
	} else {
		statusicon.setAttribute("src", "chrome://telify/content/icon18_inactive.png");
		var text = objTelifyPrefs.telStrings.getString("telify_inactive");
		statusicon.setAttribute("tooltiptext", text);
	}
},


toggleBlacklist: function()
{
	var host = objTelifyUtil.getHost();
	if (host == null) return;
	if (objTelifyPrefs.excludedHosts.indexOf(host) >= 0) {
		objTelifyUtil.arrayRemove(objTelifyPrefs.excludedHosts, host);
	} else {
		objTelifyPrefs.excludedHosts.push(host);
	}
	objTelifyPrefs.blacklist = objTelifyPrefs.excludedHosts.join(",");
	objTelifyPrefs.telPrefs.setCharPref(objTelifyPrefs.PREF_BLACKLIST, objTelifyPrefs.blacklist);
},


toggleActive: function()
{
	objTelifyPrefs.telPrefs.setBoolPref(objTelifyPrefs.PREF_ACTIVE, !objTelifyPrefs.fActive);
	this.setStatus();
},


getSelectionNumber: function()
{
	//var sel = content.window.getSelection().toString();
	var sel = document.commandDispatcher.focusedWindow.getSelection().toString();
	sel = this.convertVanityNr(sel);
	sel = objTelifyUtil.stripNumber(sel);
	return sel;
},


dialNumber: function(nr)
{
	var requ = new XMLHttpRequest();
	var url = objTelifyUtil.createDialURL(nr);

	if (objTelifyPrefs.hrefType == objTelifyPrefs.HREFTYPE_CUSTOM) {
		if (objTelifyPrefs.custom_opentype == 1) {
			window.open(url, "_blank");
			return;
		}
		if (objTelifyPrefs.custom_opentype == 2) {
			var browser = top.document.getElementById("content");
			var tab = browser.addTab(url);
			return;
		}
		if (objTelifyPrefs.custom_opentype == 3) {
			var browser = top.document.getElementById("content");
			var tab = browser.addTab(url);
			browser.selectedTab = tab;
			return;
		}
	}

	try {
		requ.open("GET", url, true);
		requ.send(null);
	} catch(e) {
		// throws exception because answer is empty (or protocol is unknown)
		if (e.name == "NS_ERROR_UNKNOWN_PROTOCOL") {
			objTelifyUtil.showMessageBox("", objTelifyLocale.msgUnknownProtocol(), objTelifyUtil.MB_ICON_ERROR);
		}
	}
},


modifyPopup: function(event)
{
	var label, key;

	//var selText = content.window.getSelection().toString();
	var selText = document.commandDispatcher.focusedWindow.getSelection().toString();

	if (document.popupNode && document.popupNode.getAttribute("class") == "telified") {
		var nr = document.popupNode.getAttribute("nr");
		var nr_parts = objTelifyUtil.splitPhoneNr(nr);
		objTelify.modifyDialPopup(nr_parts[0], nr_parts[1], "context");
		objTelifyUtil.setIdAttr("collapsed", false, "idTelify_menu_context");
	} else if (objTelifyPrefs.fActive && selText.length > 0 && objTelifyUtil.countDigits(selText) > 1) {
		var nr = objTelify.getSelectionNumber();
		var nr_parts = objTelifyUtil.splitPhoneNr(nr);
		objTelify.modifyDialPopup(nr_parts[0], nr_parts[1], "context");
		objTelifyUtil.setIdAttr("collapsed", false, "idTelify_menu_context");
	} else {
		objTelifyUtil.setIdAttr("collapsed", true, "idTelify_menu_context");
	}

	if (objTelifyPrefs.fActive) {
		label = objTelifyPrefs.telStrings.getString("telify_deactivate");
	} else {
		label = objTelifyPrefs.telStrings.getString("telify_activate");
	}
	objTelifyUtil.setIdAttr("label", label, "idTelify_menu_activity", "idTelify_status_activity");

	var host = objTelifyUtil.getHost();
	if (host) {
		objTelifyUtil.setIdAttr("disabled", !objTelifyPrefs.fActive, "idTelify_menu_blacklist", "idTelify_status_blacklist");
		if (objTelifyPrefs.excludedHosts.indexOf(host) >= 0) key = "host_active_arg"; else key = "host_inactive_arg";
		label = objTelifyUtil.substArgs(objTelifyPrefs.telStrings.getString(key), host);
		objTelifyUtil.setIdAttr("label", label, "idTelify_menu_blacklist", "idTelify_status_blacklist");
	} else {
		objTelifyUtil.setIdAttr("label", "Kein Host aktiv", "idTelify_menu_blacklist", "idTelify_status_blacklist");
		objTelifyUtil.setIdAttr("disabled", true, "idTelify_menu_blacklist", "idTelify_status_blacklist");
	}
},


showEditNumberDialog: function(cc, nr)
{
	var argObj = {cc: cc, nr: nr, fOK: false};
	window.openDialog("chrome://telify/content/editNumber.xul", "dlgTelifyEditNumber", "centerscreen,chrome,modal", argObj);
	if (argObj.fOK) {
		this.updateDialHistory(argObj.cc);
		var dial = objTelifyUtil.prefixNumber(argObj.cc, argObj.nr, "");
		objTelify.dialNumber(dial);
	}
},


dialMenuSelection: function(cc, nr)
{
	this.updateDialHistory(cc);
	var dial = objTelifyUtil.prefixNumber(cc, nr, "");
	objTelify.dialNumber(dial);
},


createTargetCountryInfo: function(prefix)
{
	var cstring = objTelifyUtil.getCountryListString(prefix);
	if (cstring) return "\n" + objTelifyPrefs.telStrings.getString('country_code') + ": " + cstring;
	return "";
},


setDialMenuItem: function(item, code, nr)
{
	var label = objTelifyUtil.prefixNumber(code, nr, "-");
	item.setAttribute("label", label);
	var cmd = "objTelify.dialMenuSelection('"+code+"','"+nr+"');";
	item.setAttribute("oncommand", cmd);
	label = objTelifyUtil.substArgs(objTelifyPrefs.telStrings.getString('call_arg'), label);
	label += objTelify.createTargetCountryInfo(code);
	item.setAttribute("tooltiptext", label);
	item.setAttribute("image", "chrome://telify/content/flag/"+code.substr(1)+".png");
},


modifyDialPopup: function(cc, nr, id)
{
	var item = document.getElementById("idTelify_"+id);
	var sep = document.getElementById("idTelify_sep_"+id);
	var numShown = 0;

	if (cc) {
		this.setDialMenuItem(item, cc, nr);
	} else {
	  item.setAttribute("label", nr);
		var label = objTelifyUtil.substArgs(objTelifyPrefs.telStrings.getString('call_arg'), nr);
		item.setAttribute("tooltiptext", label);
	  item.removeAttribute("image");
	  item.setAttribute("oncommand", "objTelify.dialNumber('"+nr+"')");
	}

	item = document.getElementById("idTelify_edit_"+id);
	if (cc) {
	  item.setAttribute("oncommand", "objTelify.showEditNumberDialog('"+cc+"','"+nr+"')");
	} else {
	  item.setAttribute("oncommand", "objTelify.showEditNumberDialog(null,'"+nr+"')");
	}

	var tldcc = objTelifyUtil.tld2cc(objTelifyUtil.getHostTLD());
	item = document.getElementById("idTelify_tld_"+id);
	if (!cc && tldcc) {
		item.setAttribute("collapsed", false);
		this.setDialMenuItem(item, tldcc, nr);
		numShown = 1;
	} else {
		item.setAttribute("collapsed", true);
		tldcc = null;
	}

	this.getDialHistory();

	if (!cc && nr.charAt(0) != '+') {
		var numLeft = objTelifyPrefs.numHistory;
		if (tldcc) numLeft--;
		for (var i=0; i<objTelifyPrefs.maxHistory; i++) {
			item = document.getElementById("idTelify_"+id+i);
			if (numLeft == 0 || this.dialHistory[i] == null || this.dialHistory[i].length == 0 || this.dialHistory[i] == cc || this.dialHistory[i] == tldcc) {
				item.setAttribute("collapsed", true);
			} else {
				item.setAttribute("collapsed", false);
				this.setDialMenuItem(item, this.dialHistory[i], nr);
				numLeft--;
				numShown++;
			}
		}
	} else {
		for (var i=0; i<objTelifyPrefs.maxHistory; i++) {
			item = document.getElementById("idTelify_"+id+i);
			item.setAttribute("collapsed", true);
		}
	}
	sep.setAttribute("collapsed", numShown == 0);
},


showDialPopup: function(target, cc, nr)
{
	var menu = document.getElementById("idTelify_popup_dial");
	var nr_parts = objTelifyUtil.splitPhoneNr(nr);
	this.modifyDialPopup(cc, nr, "dial");
	menu.openPopup(target, "after_start", 0, 0, true, false);
},


onClick: function(event)
{
	if (event.button != 0) return;
	var class = event.target.getAttribute("class");
	if (class != "telified") return;
	event.preventDefault();
	var nr = event.target.getAttribute("nr");
	var nr_parts = objTelifyUtil.splitPhoneNr(nr);
	if (event.button == 0) {
		if (nr_parts[0] && objTelifyPrefs.fDialCCDirect) {
			objTelify.dialNumber(nr);
		} else {
			objTelify.showDialPopup(event.target, nr_parts[0], nr_parts[1]);
		}
	}
	if (event.button == 2) {
		objTelify.showDialPopup(event.target, nr_parts[0], nr_parts[1]);
	}
},


getNodeBackgroundColor: function(node)
{
	node = node.parentNode;
	if (node == null) return null;
	if (node.nodeType == Node.ELEMENT_NODE) {
		var style = content.document.defaultView.getComputedStyle(node, "");
		var image = style.getPropertyValue("background-image");
		if (image && image != "none") return null;
		var color = style.getPropertyValue("background-color");
		if (color && color != "transparent") return color;
	}
	return this.getNodeBackgroundColor(node);
},


getNodeColor: function(node)
{
	node = node.parentNode;
	if (node == null) return null;
	if (node.nodeType == Node.ELEMENT_NODE) {
		var style = content.document.defaultView.getComputedStyle(node, "");
		var color = style.getPropertyValue("color");
		if (color && color != "transparent") return color;
	}
	return this.getNodeColor(node);
},


formatPhoneNr: function(phonenr)
{
	var substList = [
		["  ", " "],	// double spaces to single space
		[this.sc_nbsp, " "],	// non-breaking space to plain old space
		["+ ", "+"],	// remove space after +
		["--", "-"],	// double dashes to single dash
		["(0)", " "],	// remove optional area code prefix
		["[0]", " "],	// remove optional area code prefix
		["-/", "/"],
		["/-", "/"],
		["( ", "("],
		[" )", ")"],
		["\r", " "],
		["\n", " "],
	];

	// replace dash-like chars with dashes
	for (var i=0; i<phonenr.length; i++) {
		var c = phonenr.charAt(i);
		if (this.token_dash.indexOf(c) >= 0) {
			phonenr = phonenr.substr(0, i) + "-" + phonenr.substr(i+1);
		}
	}

	const MAXLOOP = 100; // safety bailout
	var nChanged;

	nChanged = 1;
	for (var j=0; nChanged > 0 && j < MAXLOOP; j++) {
		nChanged = 0;
		for (var i=0; i<substList.length; i++) {
			var index;
			while ((index = phonenr.indexOf(substList[i][0])) >= 0) {
				phonenr = phonenr.substr(0, index) + substList[i][1] + phonenr.substr(index+substList[i][0].length);
				nChanged++;
			}
		}
	}

	return phonenr;
},


convertVanityNr: function(phonenr)
{
	const tab_alpha = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	const tab_digit = "22233344455566677778889999";
	var newnr = "";
	for (var i=0; i<phonenr.length; i++) {
		var c = phonenr.charAt(i);
		var index = tab_alpha.indexOf(c);
		if (index >= 0) c = tab_digit.substr(index, 1);
		newnr += c;
	}
	return newnr;
},


reject: function(str, reason)
{
	if (objTelifyPrefs.fDebug == false) return;
	var msg = "Telify: reject '"+str+"' reason: "+reason;
	objTelifyUtil.logmsg(msg);
},


basechar_tab: [
	String.fromCharCode(0xa0) +
	String.fromCharCode(0x2013) +
	String.fromCharCode(0x2014) +
	String.fromCharCode(0x2212),
	" ---"
],


basechar: function(c)
{
	var index = this.basechar_tab[0].indexOf(c);
	if (index >= 0) c = this.basechar_tab[1].charAt(index);
	return c;
},


telifyTextNode: function(node)
{
	if (node == null) return 0;
	var text = node.data;
	var len = text.length;
	if (len < this.digits_min) return 0;
	var hlFactor = objTelifyPrefs.highlight/200.0;

	for (var i=0; i<len; i++) {
		var c = text.charAt(i);

		if (this.token_trigger.indexOf(c) < 0) continue;

		c = this.basechar(c);

		var str = "" + c;
		var strlen = 1;
		var last_c = c;
		var ndigits = (objTelifyUtil.isdigit(c) ? 1 : 0);
		var index;
		var fStartsWithCountryCode = false;
		var CCfromPattern = null;

		// gather allowed chars
		while (strlen < len-i) {
			c = text.charAt(i+strlen);
			c = this.basechar(c);
			if ((c == '+' && ndigits == 0) || (this.token_part.indexOf(c) >= 0)) {
				if (c == last_c && c!=' ') break;
			} else {
				if (!objTelifyUtil.isdigit(c)) break;
				ndigits++;
			}
			str += c;
			strlen++;
			last_c = c;
		}

		// check against digit count min value
		if (ndigits < this.digits_min) {
			this.reject(str, "less than "+this.digits_min+" digits");
			i += strlen - 1; continue;
		}

		// check allowed prev token
		if (i > 0) {
			var prev_c = text.charAt(i-1);
			if (this.token_disallowed_prev.indexOf(prev_c) >= 0) {
				this.reject(str, "unallowed previous token (reject list)");
				i += strlen - 1; continue;
			}
			if ((prev_c >= 'a' && prev_c <= "z") || (prev_c >= 'A' && prev_c <= "Z")) {
				this.reject(str, "unallowed previous token (letter)");
				i += strlen - 1; continue;
			}
		}

		// check if phone number starts with country code
		for (var j=0; j<telify_country_data.length; j++) {
			var cclen = telify_country_data[j][0].length;
			if (cclen < 2 || cclen > 4) continue;
			var pattern = telify_country_data[j][0].substr(1);
			var plen = pattern.length;
			if (str.substr(0, plen) != pattern) continue;
			var c = str.charAt(plen);
			if (this.token_sep.indexOf(c) < 0) continue;
			fStartsWithCountryCode = true;
			break;
		}

		// check against special local patterns
		for (var j=0; j<this.inclLocalList.length; j++) {
			var res = this.inclLocalList[j][0].exec(str);
			if (res) {CCfromPattern = this.inclLocalList[j][1]; break;}
		}

		// check if phone number starts with allowed token
		if (CCfromPattern == null && fStartsWithCountryCode == false && this.token_start.indexOf(str.charAt(0)) < 0) {
			this.reject(str, "unallowed start token (reject list)");
			i += strlen - 1; continue;
		}

		// trim chars at end of string up to an unmatched opening bracket
		index = -1;
		for (var j=strlen-1; j>=0; j--) {
			c = str.charAt(j);
			if (c == ')') break;
			if (c == '(') {index = j; break;}
		}
		if (index == 0) continue;
		if (index > 0) {
			str = str.substr(0, index);
			strlen = str.length;
		}

		// check against digit count max value (after we have removed unnecessary digits)
		if (objTelifyUtil.countDigits(str) > this.digits_max) {
			this.reject(str, "more than "+this.digits_max+" digits");
			i += strlen - 1; continue;
		}

		// trim non-digit chars at end of string
		while (str.length > 0) {
			c = str.charAt(str.length-1);
			if (!objTelifyUtil.isdigit(c)) {
				str = str.substr(0, str.length-1);
				strlen--;
			} else break;
		}

		// check allowed post token
		var post_c = text.charAt(i+strlen);
		if (post_c) {
			if (this.token_disallowed_post.indexOf(post_c) >= 0) {
				this.reject(str, "unallowed post token (reject list)");
				i += strlen - 1; continue;
			}
			if ((post_c >= 'a' && post_c <= "z") || (post_c >= 'A' && post_c <= "Z")) {
				this.reject(str, "unallowed post token (letter)");
				i += strlen - 1; continue;
			}
		}

		// check if this is just a number in braces
		// first check for unnecessary opening braces
		if (str.substr(0, 1) == "(" && str.indexOf(")") < 0) {
			str = str.substr(1);
			i++;
			strlen--;
			// now check if it still starts with allowed token
			if (this.token_start.indexOf(str.charAt(0)) < 0) {
				this.reject(str, "unallowed start token (after brace removal)");
				i += strlen - 1;
				continue;
			}
		}

		// check against blacklist patterns (date, time ranges etc.)
		index = -1;
		for (var j=0; j<this.exclPatternList.length; j++) {
			var res = this.exclPatternList[j].exec(str);
			if (res) {index = j; break;}
		}
		if (index >= 0) {this.reject(str, "blacklisted pattern #"+index); i += strlen - 1; continue;}


		// ----------------------------------------------------------------

		var display = this.formatPhoneNr(str);
		var href = objTelifyUtil.stripNumber(display);
		if (fStartsWithCountryCode) href = "+"+href;
		//if (CCfromPattern) href = CCfromPattern + href;

		// insert link into DOM

		var node_prev = content.document.createTextNode(text.substr(0, i));
		var node_after = content.document.createTextNode(text.substr(i+strlen));

		//alert("match="+str);

		var node_anchor = content.document.createElement("a");

		if (hlFactor > 0.0) {
			var color = objTelifyUtil.parseColor(this.getNodeColor(node));
			if (color == null) color = new Array(0,0,0);
			var bgcolor = objTelifyUtil.parseColor(this.getNodeBackgroundColor(node));
			if (bgcolor == null) bgcolor = new Array(255,255,255);
			for (var i=0; i<3; i++) {
				color[i] = color[i] + hlFactor * (this.hilite_color[i] - color[i]);
				bgcolor[i] = bgcolor[i] + hlFactor * (this.hilite_bgcolor[i] - bgcolor[i]);
			}
			var style = "color:#"+objTelifyUtil.color2hex(color)+";background-color:#"+objTelifyUtil.color2hex(bgcolor)+";-moz-border-radius:3px";
			node_anchor.setAttribute("style", style);
		}

		node_anchor.setAttribute("title", objTelifyPrefs.telStrings.getString('link_title'));
		node_anchor.setAttribute("class", "telified");
		node_anchor.setAttribute("nr", href);
		node_anchor.setAttribute("href", objTelifyUtil.createDialURL(href));

		var node_text = content.document.createTextNode(str);
		node_anchor.appendChild(node_text);

		var parentNode = node.parentNode;
		parentNode.replaceChild(node_after, node);
		parentNode.insertBefore(node_anchor, node_after);
		parentNode.insertBefore(node_prev, node_anchor);

		return 1;
	}

	return 0;
},


recurseNode: function(node)
{
	if (node == null) return 0; // safety
	if (node.nodeType == Node.TEXT_NODE) {
		return this.telifyTextNode(node);
	} else {
		var nChanged = 0;
		//objTelifyUtil.logmsg("node type="+node.nodeType+" "+node.tagName+" (childs:"+node.childNodes.length+")");
		if (node.nodeType == Node.ELEMENT_NODE) {
			var tagName = node.tagName.toLowerCase();
			if (objTelifyPrefs.excludedTags.indexOf(tagName) >= 0) return 0;
		}
		for (var i=0; i<node.childNodes.length; i++) {
			nChanged += this.recurseNode(node.childNodes[i]);
		}
		if (node.contentDocument) {
			nChanged += this.recurseNode(node.contentDocument.body);
			node.contentDocument.addEventListener("click", objTelify.onClick, false);
		}
	}
	return nChanged;
},


parsePage: function(event)
{
	if (!objTelifyPrefs.fActive) return;
	//objTelifyUtil.logmsg("eventPhase: "+event.eventPhase+"\n"+content.document.URL);
	if (content.document.body == null) return;
	if (event && event.eventPhase != 1) return;

	var host = objTelifyUtil.getHost();
	if (host && objTelifyPrefs.excludedHosts.indexOf(host) >= 0) return;

	//if (content.document.body.getAttribute('telified') == 1) return;
	//content.document.body.setAttribute('telified', 1);

/*
	var nChanged = 0;
	var duration = (new Date()).getTime();
	nChanged = objTelify.recurseNode(content.document.body);
	duration = (new Date()).getTime() - duration;
	var label = "Telify\n" + objTelifyPrefs.telStrings.getString('converted') + ": " + nChanged + " (" + duration + " ms)";
	document.getElementById("idTelify_statusicon").setAttribute("tooltiptext", label);
*/

	window.setTimeout("objTelify.recurseNode(content.document.body)",	0);

	content.document.addEventListener("click", objTelify.onClick, false);
},


init: function(event)
{
	window.addEventListener('load', objTelify.init, false);
	objTelifyPrefs.initTelifyPrefs();
	objTelify.setStatus();
	getBrowser().addEventListener("load", objTelify.parsePage, true);
	document.getElementById("contentAreaContextMenu").addEventListener("popupshowing", objTelify.modifyPopup, false);
	objTelifyUtil.addScheme("tel");
	objTelifyUtil.localizeCountryData();
	objTelifyUtil.getAddonVersion();
}

};


window.addEventListener('load', objTelify.init, false);

