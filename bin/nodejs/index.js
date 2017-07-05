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
 class RingDaemon{
    constructor(callbackMap){
        this.dring = require("./build/Release/dring");
        this.dring.init(callbackMap);
        var that = this;
        setInterval(function () {
            that.dring.pollEvents();
            console.log("Polling...");
        }, 10);
    }

    addAccount(Account){
        var params = new this.dring.StringMap();
        params.set("Account.type", "RING");
        params.set("Account.alias", Account.alias);
        this.dring.addAccount(params);
    }

    getAudioOutputDeviceList(){
        var devicesVect = this.dring.getAudioOutputDeviceList();
        var outputDevices = [];
        for(var i=0; i<devicesVect.size(); i++)
            outputDevices.push(devicesVect.get(i));

        return outputDevices;
    }

    getVolume(device){
        return this.dring.getVolume(device);
    }

    setVolume(device, volume){
        return this.dring.setVolume(device,volume);
    }
}

var f = function(){console.log("AccountsChanged JS");};
var daemon = new RingDaemon({"AccountsChanged": f});
//daemon.addAccount({alias:"myAlias"});
//console.log(daemon.getAudioOutputDeviceList());