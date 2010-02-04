/*
Creative Commons License: Attribution-No Derivative Works 3.0 Unported
http://creativecommons.org/licenses/by-nd/3.0/
(c)2009 Michael Koch
*/

var objTelifyMessageBox = {

init: function()
{
	var title = window.arguments[0].title;
	if (title == null || title == "") title =" Telify";
	document.getElementById("dlgTelifyMessageBox").setAttribute("title", title);
	var msg_node = document.createTextNode(window.arguments[0].msg);
	document.getElementById("idTelify_mb_msg").appendChild(msg_node);
	var flags = window.arguments[0].flags;
	if ((flags & objTelifyUtil.MB_MASK) == 0) flags |= objTelifyUtil.MB_OK; // default button
	if ((flags & objTelifyUtil.MB_OK) == 0) document.documentElement.getButton("accept").collapsed = true;
	if ((flags & objTelifyUtil.MB_CANCEL) == 0) document.documentElement.getButton("cancel").collapsed = true;
	var icon = "info32.png";
	switch (flags & objTelifyUtil.MB_ICON_MASK) {
		case objTelifyUtil.MB_ICON_ERROR: icon = "error32.png"; break;
		case objTelifyUtil.MB_ICON_WARNING: icon = "warn32.png"; break;
		case objTelifyUtil.MB_ICON_ASK: icon = "ask32.png"; break;
		case objTelifyUtil.MB_ICON_INFO: icon = "info32.png"; break;
	}
	document.getElementById("idTelify_mb_icon").setAttribute("src", "chrome://telify/content/"+icon);
},

onAccept: function()
{
	window.arguments[0].fResult = true;
	return true;
},

onCancel: function()
{
	window.arguments[0].fResult = false;
	return true;
}

};

