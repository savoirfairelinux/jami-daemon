/* (c)2009 Michael Koch
*/

var objTelifyLocale = {

openOnlineHelp: function()
{
	var browser = objTelifyUtil.getBrowser();
	var tab = browser.addTab("http://www.codepad.de/en/download/firefox-add-ons/telify.html");
	browser.selectedTab = tab;
},

msgNumberTemplateMissing: function()
{
	return "Your template does not contain a placeholder for the phone number (i.e. '$0') and will therefore not transmit a phone number. "
		+ "Do you really want to continue?";
},

msgUnknownProtocol: function()
{
	return "No application is installed which registered itself for the used protocol. "
		+ "Please configure a suitable protocol in the Telify preferences or install a suitable application.";
}

}
