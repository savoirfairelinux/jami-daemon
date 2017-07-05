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

    addAccount(account) {
        var params = new this.dring.StringMap();
        params.set("Account.type", "RING");
        if(account.archivePassword)
            params.set("Account.archivePassword", account.archivePassword);
        else {
            console.log("archivePassword required");
            return;
        }
        if(account.alias)
            params.set("Account.alias", account.alias);
        if(account.displayName)
            params.set("Account.displayName", account.displayName)
        if(account.mailbox)
            params.set("Account.mailbox", account.mailbox)
        if(account.enable)
            params.set("Account.enable", account.enable)
        if(account.autoAnswer)
            params.set("Account.autoAnswer", account.autoAnswer)
        if(account.registrationExpire)
            params.set("Account.registrationExpire", account.registrationExpire)
        if(account.dtmfType)
            params.set("Account.dtmfType", account.dtmfType)
        if(account.ringtonePath)
            params.set("Account.ringtonePath", account.ringtonePath)
        if(account.ringtoneEnabled)
            params.set("Account.ringtoneEnabled", account.ringtoneEnabled)
        if(account.videoEnabled)
            params.set("Account.videoEnabled", account.videoEnabled)
        if(account.keepAliveEnabled)
            params.set("Account.keepAliveEnabled", account.keepAliveEnabled)
        if(account.presenceEnabled)
            params.set("Account.presenceEnabled", account.presenceEnabled)
        if(account.presencePublishSupported)
            params.set("Account.presencePublishSupported", account.presencePublishSupported)
        if(account.presenceSubscribeSupported)
            params.set("Account.presenceSubscribeSupported", account.presenceSubscribeSupported)
        if(account.presenceStatus)
            params.set("Account.presenceStatus", account.presenceStatus)
        if(account.presenceNote)
            params.set("Account.presenceNote", account.presenceNote)
        if(account.hostname)
            params.set("Account.hostname", account.hostname)
        if(account.username)
            params.set("Account.username", account.username)
        if(account.routeset)
            params.set("Account.routeset", account.routeset)
        if(account.password)
            params.set("Account.password", account.password)
        if(account.realm)
            params.set("Account.realm", account.realm)
        if(account.useragent)
            params.set("Account.useragent", account.useragent)
        if(account.hasCustomUserAgent)
            params.set("Account.hasCustomUserAgent", account.hasCustomUserAgent)
        if(account.audioPortMin)
            params.set("Account.audioPortMin", account.audioPortMin)
        if(account.audioPortMax)
            params.set("Account.audioPortMax", account.audioPortMax)
        if(account.videoPortMin)
            params.set("Account.videoPortMin", account.videoPortMin)
        if(account.videoPortMax)
            params.set("Account.videoPortMax", account.videoPortMax)
        if(account.localInterface)
            params.set("Account.localInterface", account.localInterface)
        if(account.publishedSameAsLocal)
            params.set("Account.publishedSameAsLocal", account.publishedSameAsLocal)
        if(account.localPort)
            params.set("Account.localPort", account.localPort)
        if(account.publishedPort)
            params.set("Account.publishedPort", account.publishedPort)
        if(account.publishedAddress)
            params.set("Account.publishedAddress", account.publishedAddress)
        if(account.upnpEnabled)
            params.set("Account.upnpEnabled", account.upnpEnabled)

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