/* (c)2009 Michael Koch
*/

var objTelifyLocale = {

openOnlineHelp: function()
{
	var browser = objTelifyUtil.getBrowser();
	var tab = browser.addTab("http://www.codepad.de/de/download/firefox-add-ons/telify.html");
	browser.selectedTab = tab;
},

msgNumberTemplateMissing: function()
{
	return "Ihre Vorlage enthält keinen Platzhalter für die Telefonnummer (d.h. '$0') und wird deshalb keine Telefonnummer übermitteln. "
		+ "Wollen Sie das wirklich?";
},

msgUnknownProtocol: function()
{
	return "Im diesem System ist keine Anwendung installiert, die sich für das verwendete Protokoll registriert hat. "
		+ "Bitte stellen Sie in der Telify-Konfiguration ein geeignetes Protokoll ein oder installieren Sie eine geeignete Anwendung.";
}

}
