'use strict';
const dring = require("./build/Release/dring")

dring['setAccountsChangedCb'](function(){
	console.log("accountsChanged Called | JavaScript");
});

dring['setRegistrationStateChangedCb'](function(account_id,state,code,detail_str){
	console.log("registrationStateChanged Called | JavaScript");
	console.log(account_id+"|"+state+"|"+code+"|"+detail_str);
});

dring.init();


var params = new dring.StringMap();
params.set("Account.type","RING");
params.set("Account.alias","RingAccount");
dring.addAccount(params);

setInterval(function(){
	dring.pollEvents();
	//console.log("Polling...");
},10);
