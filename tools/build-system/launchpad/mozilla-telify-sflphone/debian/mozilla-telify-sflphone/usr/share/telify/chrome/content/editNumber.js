/*
Creative Commons License: Attribution-No Derivative Works 3.0 Unported
http://creativecommons.org/licenses/by-nd/3.0/
(c)2009 Michael Koch
*/

var objTelifyEditNumber = {

checkKey: function(event, allowed)
{
	if (event.which < 32) return
	var key = String.fromCharCode(event.which)
	if (allowed.indexOf(key) >= 0) return;
	event.preventDefault();
},


createListItem: function()
{
	var item = document.createElement('listitem');
	for (var i=0; i<arguments.length; i++) {
		var cell = document.createElement('listcell');
		cell.setAttribute("label", arguments[i]);
		item.appendChild(cell);
	}
	return item;
},


updateCountrySelection: function()
{
	var list = document.getElementById("idTelifyCountryCodeList");
	var editcc = document.getElementById("idTelifyInputCC");
	if (editcc.value == "" || editcc.value == "+" || editcc.value.charAt(0) != '+') {
		list.scrollToIndex(0);
		list.selectedIndex = 0;
		editcc.style.color = "#ff0000";
		return;
	}
	var index = 0;
	var maxlen = 1;
	for (var i=0; i<telify_country_data.length; i++) {
		if (editcc.value == telify_country_data[i][0]) {
			index = i;
			break;
		}
		for (var j=1; j<editcc.value.length; j++) {
			if (editcc.value.charAt(j) == telify_country_data[i][0].charAt(j)) {
				if (j+1 > maxlen) {
					maxlen = j+1;
					index = i;
				}
			} else {
				break;
			}
		}
	}
	if (index >= 0) {
		list.scrollToIndex(index);
		if (editcc.value == telify_country_data[index][0]) {
			list.selectedIndex = index;
			editcc.style.color = "#000000";
		} else {
			list.clearSelection();
			editcc.style.color = "#ff0000";
		}
	} else {
		list.scrollToIndex(0);
		list.clearSelection();
		editcc.style.color = "#ff0000";
	}
},


ccChanged: function()
{
	var editcc = document.getElementById("idTelifyInputCC");
	if (editcc.value.length == 1 && editcc.value.charAt(0) != '+') {
		editcc.value = "+" + editcc.value;
	}
	this.updateCountrySelection();
},


updateNumberEdit: function()
{
	var list = document.getElementById("idTelifyCountryCodeList");
	var fClear = false;
	if (list.getRowCount() != telify_country_data.length) {
		while (list.getRowCount() > 0) list.removeItemAt(0);
		fClear = true;
	}
	for (var i=0; i<telify_country_data.length; i++) {
		var item = this.createListItem(telify_country_data[i][0], telify_country_data[i][1]);
		if (fClear) {
			list.appendChild(item);
		} else {
			list.replaceChild(item, list.getItemAtIndex(i));
		}
	}
	this.updateCountrySelection();
},


updateListSelection: function()
{
	var list = document.getElementById("idTelifyCountryCodeList");
	var editcc = document.getElementById("idTelifyInputCC");
	if (list.selectedCount > 0) {
		editcc.value = telify_country_data[list.selectedIndex][0];
		editcc.style.color = "#000000";
	}
},


compareCol1: function(a, b)
{
	var v = a[0].localeCompare(b[0]);
	if (v == 0) return a[1].localeCompare(b[1]);
	return v;
},


compareCol2: function(a, b)
{
	var v = a[1].localeCompare(b[1]);
	if (v == 0) return a[0].localeCompare(b[0]);
	return v;
},


last_sorted_column: -1,

sortCountryCodeList: function(column)
{
	var telPrefs = objTelifyPrefs.getPrefObj();
	if (column < 0) {
		column = telPrefs.getIntPref(objTelifyPrefs.PREF_COLSORTCC);
	} else {
		telPrefs.setIntPref(objTelifyPrefs.PREF_COLSORTCC, column);
	}
	if (column == this.last_sorted_column) return;
	if (column == 0) {
		telify_country_data.sort(this.compareCol1);
		document.getElementById("idTelifyColCode").setAttribute("sortDirection", "descending");
		document.getElementById("idTelifyColCountry").setAttribute("sortDirection", "natural");
	}
	if (column == 1) {
		telify_country_data.sort(this.compareCol2);
		document.getElementById("idTelifyColCode").setAttribute("sortDirection", "natural");
		document.getElementById("idTelifyColCountry").setAttribute("sortDirection", "descending");
	}
	this.last_sorted_column = column;
	this.updateNumberEdit();
},


setNumberEditReturnValue: function(fOK)
{
	window.arguments[0].cc = document.getElementById("idTelifyInputCC").value;
	window.arguments[0].nr = document.getElementById("idTelifyInputNr").value;
	window.arguments[0].fOK = fOK;
},


initNumberEdit: function()
{
	var cc = window.arguments[0].cc;
	var nr = window.arguments[0].nr;
	var index = -1;
	var maxlen = 0;

	objTelifyUtil.localizeCountryData();
	document.getElementById("idTelifyInputCC").value = (cc ? cc : "");
	document.getElementById("idTelifyInputNr").value = nr;
	this.sortCountryCodeList(-1);
}

};

