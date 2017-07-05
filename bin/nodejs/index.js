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
    constructor(callbackMap) {
        this.dring = require("./build/Release/dring");
        this.dring.init(callbackMap);
        var that = this;
        this.pollIntervalId = setInterval(function () {
            that.dring.pollEvents();
            console.log("Polling...");
        }, 10);
    }

    boolToStr(bool){
        if(bool)
            return "TRUE";
        else
            return "FALSE";
    }

    addAccount(account) {
        var params = new this.dring.StringMap();
        params.set("Account.type", "RING");
        if(account.archivePassword){
            params.set("Account.archivePassword", account.archivePassword);
        } else {
            console.log("archivePassword required");
            return;
        }
        if(account.alias)
            params.set("Account.alias", account.alias);
        if(account.displayName)
            params.set("Account.displayName", account.displayName);
        if(account.enable)
            params.set("Account.enable", this.boolToStr(account.enable));
        if(account.autoAnswer)
            params.set("Account.autoAnswer", this.boolToStr(account.autoAnswer));
        if(account.ringtonePath)
            params.set("Account.ringtonePath", account.ringtonePath);
        if(account.ringtoneEnabled)
            params.set("Account.ringtoneEnabled", this.boolToStr(account.ringtoneEnabled));
        if(account.videoEnabled)
            params.set("Account.videoEnabled", this.boolToStr(account.videoEnabled));
        if(account.useragent){
            params.set("Account.useragent", account.useragent);
            params.set("Account.hasCustomUserAgent","TRUE");
        } else {
            params.set("Account.hasCustomUserAgent","FALSE");
        }
        if(account.audioPortMin)
            params.set("Account.audioPortMin", account.audioPortMin);
        if(account.audioPortMax)
            params.set("Account.audioPortMax", account.audioPortMax);
        if(account.videoPortMin)
            params.set("Account.videoPortMin", account.videoPortMin);
        if(account.videoPortMax)
            params.set("Account.videoPortMax", account.videoPortMax);
        if(account.localInterface)
            params.set("Account.localInterface", account.localInterface);
        if(account.publishedSameAsLocal)
            params.set("Account.publishedSameAsLocal", this.boolToStr(account.publishedSameAsLocal));
        if(account.localPort)
            params.set("Account.localPort", account.localPort);
        if(account.publishedPort)
            params.set("Account.publishedPort", account.publishedPort);
        if(account.publishedAddress)
            params.set("Account.publishedAddress", account.publishedAddress);
        if(account.upnpEnabled)
            params.set("Account.upnpEnabled", this.boolToStr(account.upnpEnabled));

        this.dring.addAccount(params);
    }

    getAudioOutputDeviceList() {
        var devicesVect = this.dring.getAudioOutputDeviceList();
        var outputDevices = [];
        for(var i=0; i<devicesVect.size(); i++)
            outputDevices.push(devicesVect.get(i));

        return outputDevices;
    }

    getVolume(deviceName) {
        return this.dring.getVolume(deviceName);
    }

    setVolume(deviceName, volume) {
        return this.dring.setVolume(deviceName,volume);
    }

    stop() {
        clearInterval(this.pollIntervalId);
        this.dring.fini();
    }
}

var f = function(){ console.log("AccountsChanged JS"); };
var daemon = new RingDaemon({"AccountsChanged": f});
//daemon.addAccount({alias:"myAlias", archivePassword:"pass"});
//console.log(daemon.getAudioOutputDeviceList());
daemon.stop();