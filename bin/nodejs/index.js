/*
 *  Copyright (c) 2017 Savoir-faire Linux Inc.
 *
 *  Author: Adrien Béraud <adrien.beraud@savoirfairelinux.com>
 *  Author: Asad Salman <me@asad.co>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program. If not, see <https://www.gnu.org/licenses/>.
 */
"use strict";
const dring = require("./build/Release/dring");

dring.setAccountsChangedCb(function () {
    console.log("accountsChanged Called | JavaScript");
});

dring.setRegistrationStateChangedCb(function (account_id, state, code, detail_str) {
    console.log("registrationStateChanged Called | JavaScript");
    console.log(account_id + "|" + state + "|" + code + "|" + detail_str);
});

dring.init();

var params = new dring.StringMap();
params.set("Account.type", "RING");
params.set("Account.alias", "RingAccount");
dring.addAccount(params);

setInterval(function () {
    dring.pollEvents();
    //console.log("Polling...");
}, 10);
