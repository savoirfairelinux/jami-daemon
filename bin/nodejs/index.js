/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
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
class JamiDaemon {
    constructor(callbackMap) {
        if (callbackMap){
            this.jami = require("./build/Release/jami.node");
            this.jami.init(callbackMap);
        }
    }

    boolToStr(bool) {
        return bool ? JamiDaemon.BOOL_TRUE : JamiDaemon.BOOL_FALSE;
    }

    addAccount(account) {
        const params = new this.jami.StringMap();
        params.set("Account.type", "RING");
        if (account.managerUri)
            params.set("Account.managerUri", account.managerUri);
        if (account.managerUsername)
            params.set("Account.managerUsername", account.managerUsername);
        if (account.archivePassword) {
            params.set("Account.archivePassword", account.archivePassword);
        } else {
            console.log("archivePassword required");
            return;
        }
        if (account.alias)
            params.set("Account.alias", account.alias);
        if (account.displayName)
            params.set("Account.displayName", account.displayName);
        if (account.enable)
            params.set("Account.enable", this.boolToStr(account.enable));
        if (account.autoAnswer)
            params.set("Account.autoAnswer", this.boolToStr(account.autoAnswer));
        if (account.ringtonePath)
            params.set("Account.ringtonePath", account.ringtonePath);
        if (account.ringtoneEnabled)
            params.set("Account.ringtoneEnabled", this.boolToStr(account.ringtoneEnabled));
        if (account.videoEnabled)
            params.set("Account.videoEnabled", this.boolToStr(account.videoEnabled));
        if (account.useragent) {
            params.set("Account.useragent", account.useragent);
            params.set("Account.hasCustomUserAgent", JamiDaemon.BOOL_TRUE);
        } else {
            params.set("Account.hasCustomUserAgent", JamiDaemon.BOOL_FALSE);
        }
        if (account.audioPortMin)
            params.set("Account.audioPortMin", account.audioPortMin);
        if (account.audioPortMax)
            params.set("Account.audioPortMax", account.audioPortMax);
        if (account.videoPortMin)
            params.set("Account.videoPortMin", account.videoPortMin);
        if (account.videoPortMax)
            params.set("Account.videoPortMax", account.videoPortMax);
        if (account.localInterface)
            params.set("Account.localInterface", account.localInterface);
        if (account.publishedSameAsLocal)
            params.set("Account.publishedSameAsLocal", this.boolToStr(account.publishedSameAsLocal));
        if (account.localPort)
            params.set("Account.localPort", account.localPort);
        if (account.publishedPort)
            params.set("Account.publishedPort", account.publishedPort);
        if (account.publishedAddress)
            params.set("Account.publishedAddress", account.publishedAddress);
        if (account.upnpEnabled)
            params.set("Account.upnpEnabled", this.boolToStr(account.upnpEnabled));

        this.jami.addAccount(params);
    }
    stringVectToArr(stringvect) {
        const outputArr = [];
        for (let i = 0; i < stringvect.size(); i++)
            outputArr.push(stringvect.get(i));
        return outputArr;
    }
    mapToJs(m) {
        const outputObj = {};
        this.stringVectToArr(m.keys())
            .forEach(k => outputObj[k] = m.get(k));
        return outputObj;
    }
    getAccountList() {
        return this.stringVectToArr(this.jami.getAccountList());
    }
    getAccountDetails(accountId) {
        return this.mapToJs(this.jami.getAccountDetails(accountId));
    }
    getAudioOutputDeviceList() {
        return this.stringVectToArr(this.jami.getAudioOutputDeviceList());
    }
    getVolume(deviceName) {
        return this.jami.getVolume(deviceName);
    }

    setVolume(deviceName, volume) {
        return this.jami.setVolume(deviceName, volume);
    }

    stop() {
        this.jami.fini();
    }
}

JamiDaemon.BOOL_TRUE = "true"
JamiDaemon.BOOL_FALSE = "false"

module.exports = JamiDaemon;
