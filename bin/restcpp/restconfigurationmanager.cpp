/*
 *  Copyright (C) 2016-2019 Savoir-faire Linux Inc.
 *
 *  Author: Simon Zeni <simon.zeni@savoirfairelinux.com>
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
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
 */
#include "restconfigurationmanager.h"

RestConfigurationManager::RestConfigurationManager() :
    resources_()
{
    // We start by filling the resources_ array with all the resources
    populateResources();
}

std::vector<std::shared_ptr<restbed::Resource>>
RestConfigurationManager::getResources()
{
    return resources_;
}

// Private

std::map<std::string, std::string>
RestConfigurationManager::parsePost(const std::string& post)
{
    // Simple function to parse a POST request like "param1=value1&param2=value2"
    std::map<std::string, std::string> data;

    auto split = [](const std::string& s, char delim){
        std::vector<std::string> v;
        auto i = 0;
        auto pos = s.find(delim);
        while (pos != std::string::npos)
        {
            v.push_back(s.substr(i, pos-i));
            i = ++pos;
            pos = s.find(delim, pos);

            if (pos == std::string::npos)
                v.push_back(s.substr(i, s.length()));
        }

        return v;
    };

    if(post.find_first_of('&') != std::string::npos)
    {
        std::vector<std::string> v = split(post, '&');

        for(auto& it : v)
        {
            std::vector<std::string> tmp = split(it, '=');
            data[tmp.front()] = tmp.back();
        }
    }
    else
    {
        std::vector<std::string> tmp = split(post, '=');
        data[tmp.front()] = tmp.back();
    }

    return data;
}

void
RestConfigurationManager::populateResources()
{
    // This function is atrociously long and redundant, but it works. Sorry

    resources_.push_back(std::make_shared<restbed::Resource>());
    resources_.back()->set_path("/configurationManager");
    resources_.back()->set_method_handler("GET",
        std::bind(&RestConfigurationManager::defaultRoute, this, std::placeholders::_1));

    resources_.push_back(std::make_shared<restbed::Resource>());
    resources_.back()->set_path("/accountDetails/{accountID: [a-z0-9]*}");
    resources_.back()->set_method_handler("GET",
        std::bind(&RestConfigurationManager::getAccountDetails, this, std::placeholders::_1));

    resources_.push_back(std::make_shared<restbed::Resource>());
    resources_.back()->set_path("/volatileAccountDetails/{accountID: [a-z0-9]*}");
    resources_.back()->set_method_handler("GET",
        std::bind(&RestConfigurationManager::getVolatileAccountDetails, this, std::placeholders::_1));

    resources_.push_back(std::make_shared<restbed::Resource>());
    resources_.back()->set_path("/setAccountDetails/{accountID: [a-z0-9]*}");
    resources_.back()->set_method_handler("POST",
        std::bind(&RestConfigurationManager::setAccountDetails, this, std::placeholders::_1));

    resources_.push_back(std::make_shared<restbed::Resource>());
    resources_.back()->set_path("/registerName/{accountID: [a-z0-9]*}");
    resources_.back()->set_method_handler("POST",
        std::bind(&RestConfigurationManager::registerName, this, std::placeholders::_1));

    resources_.push_back(std::make_shared<restbed::Resource>());
    resources_.back()->set_path("/lookupName/{name: [a-z0-9]*}");
    resources_.back()->set_method_handler("GET",
        std::bind(&RestConfigurationManager::lookupName, this, std::placeholders::_1));

    resources_.push_back(std::make_shared<restbed::Resource>());
    resources_.back()->set_path("/setAccountActive/{accountID: [a-z0-9]*}/{status: (true|false)}");
    resources_.back()->set_method_handler("GET",
        std::bind(&RestConfigurationManager::setAccountActive, this, std::placeholders::_1));

    resources_.push_back(std::make_shared<restbed::Resource>());
    resources_.back()->set_path("/accountTemplate/{type: [a-zA-Z]*}");
    resources_.back()->set_method_handler("GET",
        std::bind(&RestConfigurationManager::getAccountTemplate, this, std::placeholders::_1));

    resources_.push_back(std::make_shared<restbed::Resource>());
    resources_.back()->set_path("/addAccount");
    resources_.back()->set_method_handler("POST",
        std::bind(&RestConfigurationManager::addAccount, this, std::placeholders::_1));

    resources_.push_back(std::make_shared<restbed::Resource>());
    resources_.back()->set_path("/removeAccount/{accountID: [a-z0-9]*}");
    resources_.back()->set_method_handler("GET",
        std::bind(&RestConfigurationManager::removeAccount, this, std::placeholders::_1));

    resources_.push_back(std::make_shared<restbed::Resource>());
    resources_.back()->set_path("/accountList");
    resources_.back()->set_method_handler("GET",
        std::bind(&RestConfigurationManager::getAccountList, this, std::placeholders::_1));

    resources_.push_back(std::make_shared<restbed::Resource>());
    resources_.back()->set_path("/sendRegister/{accountID: [a-z0-9]*}/{status: (true|false)}");
    resources_.back()->set_method_handler("GET",
        std::bind(&RestConfigurationManager::sendRegister, this, std::placeholders::_1));

    resources_.push_back(std::make_shared<restbed::Resource>());
    resources_.back()->set_path("/registerAllAccounts");
    resources_.back()->set_method_handler("GET",
        std::bind(&RestConfigurationManager::registerAllAccounts, this, std::placeholders::_1));

    resources_.push_back(std::make_shared<restbed::Resource>());
    resources_.back()->set_path("/sendTextMessage/{accountID: [a-z0-9]*}/{to: [a-z0-9]*}");
    resources_.back()->set_method_handler("POST",
        std::bind(&RestConfigurationManager::sendTextMessage, this, std::placeholders::_1));

    resources_.push_back(std::make_shared<restbed::Resource>());
    resources_.back()->set_path("/messageStatus/{id: [0-9]*}");
    resources_.back()->set_method_handler("GET",
        std::bind(&RestConfigurationManager::getMessageStatus, this, std::placeholders::_1));

    resources_.push_back(std::make_shared<restbed::Resource>());
    resources_.back()->set_path("/tlsDefaultSettings");
    resources_.back()->set_method_handler("GET",
        std::bind(&RestConfigurationManager::getTlsDefaultSettings, this, std::placeholders::_1));

    resources_.push_back(std::make_shared<restbed::Resource>());
    resources_.back()->set_path("/codecList");
    resources_.back()->set_method_handler("GET",
        std::bind(&RestConfigurationManager::getCodecList, this, std::placeholders::_1));

    resources_.push_back(std::make_shared<restbed::Resource>());
    resources_.back()->set_path("/supportedTlsMethod");
    resources_.back()->set_method_handler("GET",
        std::bind(&RestConfigurationManager::getSupportedTlsMethod, this, std::placeholders::_1));

    resources_.push_back(std::make_shared<restbed::Resource>());
    resources_.back()->set_path("/supportedCiphers/{accountID: [a-z0-9]*}");
    resources_.back()->set_method_handler("GET",
        std::bind(&RestConfigurationManager::getSupportedCiphers, this, std::placeholders::_1));

    resources_.push_back(std::make_shared<restbed::Resource>());
    resources_.back()->set_path("/codecDetails/{accountID: [a-z0-9]*}/{codecID: [0-9]*}");
    resources_.back()->set_method_handler("GET",
        std::bind(&RestConfigurationManager::getCodecDetails, this, std::placeholders::_1));

    resources_.push_back(std::make_shared<restbed::Resource>());
    resources_.back()->set_path("/setCodecDetails/{accountID: [a-z0-9]*}/{codecID: [0-9]*}");
    resources_.back()->set_method_handler("POST",
        std::bind(&RestConfigurationManager::getCodecDetails, this, std::placeholders::_1));

    resources_.push_back(std::make_shared<restbed::Resource>());
    resources_.back()->set_path("/activeCodecList/{accountID: [a-z0-9]*}");
    resources_.back()->set_method_handler("GET",
        std::bind(&RestConfigurationManager::getActiveCodecList, this, std::placeholders::_1));

    resources_.push_back(std::make_shared<restbed::Resource>());
    resources_.back()->set_path("/audioPluginList");
    resources_.back()->set_method_handler("GET",
        std::bind(&RestConfigurationManager::getAudioPluginList, this, std::placeholders::_1));

    resources_.push_back(std::make_shared<restbed::Resource>());
    resources_.back()->set_path("/setAudioPlugin/{plugin: [a-zA-Z0-9]*}");
    resources_.back()->set_method_handler("GET",
        std::bind(&RestConfigurationManager::setAudioPlugin, this, std::placeholders::_1));
    ///
    resources_.push_back(std::make_shared<restbed::Resource>());
    resources_.back()->set_path("/audioOutputDeviceList");
    resources_.back()->set_method_handler("GET",
        std::bind(&RestConfigurationManager::getAudioOutputDeviceList, this, std::placeholders::_1));

    resources_.push_back(std::make_shared<restbed::Resource>());
    resources_.back()->set_path("/setAudioOutputDevice/{index: [0-9]*}");
    resources_.back()->set_method_handler("GET",
        std::bind(&RestConfigurationManager::setAudioOutputDevice, this, std::placeholders::_1));

    resources_.push_back(std::make_shared<restbed::Resource>());
    resources_.back()->set_path("/setAudioInputDevice/{index: [0-9]*}");
    resources_.back()->set_method_handler("GET",
        std::bind(&RestConfigurationManager::setAudioInputDevice, this, std::placeholders::_1));

    resources_.push_back(std::make_shared<restbed::Resource>());
    resources_.back()->set_path("/setAudioRingtoneDevice/{plugin: [0-9]*}");
    resources_.back()->set_method_handler("GET",
        std::bind(&RestConfigurationManager::setAudioRingtoneDevice, this, std::placeholders::_1));

    resources_.push_back(std::make_shared<restbed::Resource>());
    resources_.back()->set_path("/audioInputDeviceList");
    resources_.back()->set_method_handler("GET",
        std::bind(&RestConfigurationManager::getAudioInputDeviceList, this, std::placeholders::_1));

    resources_.push_back(std::make_shared<restbed::Resource>());
    resources_.back()->set_path("/currentAudioDeviceIndex");
    resources_.back()->set_method_handler("GET",
        std::bind(&RestConfigurationManager::getCurrentAudioDevicesIndex, this, std::placeholders::_1));

    resources_.push_back(std::make_shared<restbed::Resource>());
    resources_.back()->set_path("/audioInputDeviceIndex/{name: [a-zA-Z0-9]*}");
    resources_.back()->set_method_handler("GET",
        std::bind(&RestConfigurationManager::getAudioInputDeviceIndex, this, std::placeholders::_1));

    resources_.push_back(std::make_shared<restbed::Resource>());
    resources_.back()->set_path("/audioOutputDeviceIndex/{name: [a-zA-Z0-9]*}");
    resources_.back()->set_method_handler("GET",
        std::bind(&RestConfigurationManager::getAudioInputDeviceIndex, this, std::placeholders::_1));

    resources_.push_back(std::make_shared<restbed::Resource>());
    resources_.back()->set_path("/currentAudioOutputPlugin");
    resources_.back()->set_method_handler("GET",
        std::bind(&RestConfigurationManager::getCurrentAudioOutputPlugin, this, std::placeholders::_1));

    resources_.push_back(std::make_shared<restbed::Resource>());
    resources_.back()->set_path("/noiseSuppressState");
    resources_.back()->set_method_handler("GET",
        std::bind(&RestConfigurationManager::getNoiseSuppressState, this, std::placeholders::_1));

    resources_.push_back(std::make_shared<restbed::Resource>());
    resources_.back()->set_path("/setNoiseSuppressState/{state: (true|false)");
    resources_.back()->set_method_handler("GET",
        std::bind(&RestConfigurationManager::setNoiseSuppressState, this, std::placeholders::_1));

    resources_.push_back(std::make_shared<restbed::Resource>());
    resources_.back()->set_path("/isAgcEnable");
    resources_.back()->set_method_handler("GET",
        std::bind(&RestConfigurationManager::isAgcEnabled, this, std::placeholders::_1));

    resources_.push_back(std::make_shared<restbed::Resource>());
    resources_.back()->set_path("/setAgcState/{state: (true|false)}");
    resources_.back()->set_method_handler("GET",
        std::bind(&RestConfigurationManager::setAgcState, this, std::placeholders::_1));

    resources_.push_back(std::make_shared<restbed::Resource>());
    resources_.back()->set_path("/muteDtmf/{state: (true|false)}");
    resources_.back()->set_method_handler("GET",
        std::bind(&RestConfigurationManager::muteDtmf, this, std::placeholders::_1));

    resources_.push_back(std::make_shared<restbed::Resource>());
    resources_.back()->set_path("/isDtmfMuted");
    resources_.back()->set_method_handler("GET",
        std::bind(&RestConfigurationManager::isDtmfMuted, this, std::placeholders::_1));

    resources_.push_back(std::make_shared<restbed::Resource>());
    resources_.back()->set_path("/isCaptureMuted");
    resources_.back()->set_method_handler("GET",
        std::bind(&RestConfigurationManager::isCaptureMuted, this, std::placeholders::_1));

    resources_.push_back(std::make_shared<restbed::Resource>());
    resources_.back()->set_path("/muteCapture/{state: (true|false)}");
    resources_.back()->set_method_handler("GET",
        std::bind(&RestConfigurationManager::muteCapture, this, std::placeholders::_1));

    resources_.push_back(std::make_shared<restbed::Resource>());
    resources_.back()->set_path("/isPlaybackMuted");
    resources_.back()->set_method_handler("GET",
        std::bind(&RestConfigurationManager::isPlaybackMuted, this, std::placeholders::_1));

    resources_.push_back(std::make_shared<restbed::Resource>());
    resources_.back()->set_path("/mutePlayback/{state: (true|false)}");
    resources_.back()->set_method_handler("GET",
        std::bind(&RestConfigurationManager::mutePlayback, this, std::placeholders::_1));

    resources_.push_back(std::make_shared<restbed::Resource>());
    resources_.back()->set_path("/isRingtoneMuted");
    resources_.back()->set_method_handler("GET",
        std::bind(&RestConfigurationManager::isRingtoneMuted, this, std::placeholders::_1));

    resources_.push_back(std::make_shared<restbed::Resource>());
    resources_.back()->set_path("/muteRingtone/{state: (true|false)}");
    resources_.back()->set_method_handler("GET",
        std::bind(&RestConfigurationManager::muteRingtone, this, std::placeholders::_1));

    resources_.push_back(std::make_shared<restbed::Resource>());
    resources_.back()->set_path("/audioManager");
    resources_.back()->set_method_handler("GET",
        std::bind(&RestConfigurationManager::getAudioManager, this, std::placeholders::_1));

    resources_.push_back(std::make_shared<restbed::Resource>());
    resources_.back()->set_path("/setAudioManager/{api: [a-zA-Z0-9]*}");
    resources_.back()->set_method_handler("GET",
        std::bind(&RestConfigurationManager::setAudioManager, this, std::placeholders::_1));

    resources_.push_back(std::make_shared<restbed::Resource>());
    resources_.back()->set_path("/supportedAudioManager");
    resources_.back()->set_method_handler("GET",
        std::bind(&RestConfigurationManager::getSupportedAudioManagers, this, std::placeholders::_1));

    resources_.push_back(std::make_shared<restbed::Resource>());
    resources_.back()->set_path("/recordPath");
    resources_.back()->set_method_handler("GET",
        std::bind(&RestConfigurationManager::getRecordPath, this, std::placeholders::_1));

    resources_.push_back(std::make_shared<restbed::Resource>());
    resources_.back()->set_path("/setRecordPath/{path: (\\/[a-zA-Z0-9\\.]{1,}){1,}([a-zA-Z0-9]*\\.[a-zA-Z]*)}");
    resources_.back()->set_method_handler("GET",
        std::bind(&RestConfigurationManager::setRecordPath, this, std::placeholders::_1));

    resources_.push_back(std::make_shared<restbed::Resource>());
    resources_.back()->set_path("/isAlwaysRecording");
    resources_.back()->set_method_handler("GET",
        std::bind(&RestConfigurationManager::getIsAlwaysRecording, this, std::placeholders::_1));

    resources_.push_back(std::make_shared<restbed::Resource>());
    resources_.back()->set_path("/setIsAlwaysRecording/{status: (true|false)}");
    resources_.back()->set_method_handler("GET",
        std::bind(&RestConfigurationManager::setIsAlwaysRecording, this, std::placeholders::_1));

    resources_.push_back(std::make_shared<restbed::Resource>());
    resources_.back()->set_path("/setHistoryLimit/{limit: [0-9]*}");
    resources_.back()->set_method_handler("GET",
        std::bind(&RestConfigurationManager::setHistoryLimit, this, std::placeholders::_1));

    resources_.push_back(std::make_shared<restbed::Resource>());
    resources_.back()->set_path("/getHistoryLimit");
    resources_.back()->set_method_handler("GET",
        std::bind(&RestConfigurationManager::getHistoryLimit, this, std::placeholders::_1));

    resources_.push_back(std::make_shared<restbed::Resource>());
    resources_.back()->set_path("/setAccountsOrder");
    resources_.back()->set_method_handler("POST",
        std::bind(&RestConfigurationManager::setAccountsOrder, this, std::placeholders::_1));

    resources_.push_back(std::make_shared<restbed::Resource>());
    resources_.back()->set_path("/hookSettings");
    resources_.back()->set_method_handler("GET",
        std::bind(&RestConfigurationManager::getHookSettings, this, std::placeholders::_1));

    resources_.push_back(std::make_shared<restbed::Resource>());
    resources_.back()->set_path("/setHookSettings");
    resources_.back()->set_method_handler("POST",
        std::bind(&RestConfigurationManager::setHookSettings, this, std::placeholders::_1));

    resources_.push_back(std::make_shared<restbed::Resource>());
    resources_.back()->set_path("/credentials/{accountID: [a-z0-9]*}");
    resources_.back()->set_method_handler("GET",
        std::bind(&RestConfigurationManager::getCredentials, this, std::placeholders::_1));

    resources_.push_back(std::make_shared<restbed::Resource>());
    resources_.back()->set_path("/setCredentials/{accountID: [a-z0-9]*}");
    resources_.back()->set_method_handler("POST",
        std::bind(&RestConfigurationManager::setCredentials, this, std::placeholders::_1));

    resources_.push_back(std::make_shared<restbed::Resource>());
    resources_.back()->set_path("/addrFromInterfaceName/{interface: [a-zA-Z0-9]}");
    resources_.back()->set_method_handler("GET",
        std::bind(&RestConfigurationManager::getAddrFromInterfaceName, this, std::placeholders::_1));

    resources_.push_back(std::make_shared<restbed::Resource>());
    resources_.back()->set_path("/allIpInterface");
    resources_.back()->set_method_handler("GET",
        std::bind(&RestConfigurationManager::getAllIpInterface, this, std::placeholders::_1));

    resources_.push_back(std::make_shared<restbed::Resource>());
    resources_.back()->set_path("/allIpInterfaceByName");
    resources_.back()->set_method_handler("GET",
        std::bind(&RestConfigurationManager::getAllIpInterfaceByName, this, std::placeholders::_1));

    resources_.push_back(std::make_shared<restbed::Resource>());
    resources_.back()->set_path("/shortcuts");
    resources_.back()->set_method_handler("GET",
        std::bind(&RestConfigurationManager::getShortcuts, this, std::placeholders::_1));

    resources_.push_back(std::make_shared<restbed::Resource>());
    resources_.back()->set_path("/setShortcuts");
    resources_.back()->set_method_handler("POST",
        std::bind(&RestConfigurationManager::setShortcuts, this, std::placeholders::_1));
}

void
RestConfigurationManager::defaultRoute(const std::shared_ptr<restbed::Session> session)
{
    RING_INFO("[%s] GET /configurationManager", session->get_origin().c_str());

    std::string body = "Available routes are : \r\n";
    body += "GET /accountDetails/{accountID: [a-z0-9]*}\r\n";
    body += "GET /volatileAccountDetails/{accountID: [a-z0-9]*}\r\n";
    body += "POST /setAccountDetails/{accountID: [a-z0-9]*}\r\n";
    body += "POST /registerName/{accountID: [a-z0-9]*}\r\n";
    body += "GET /setAccountActive/{accountID: [a-z0-9]*}/{status: (true|false)}\r\n";
    body += "GET /accountTemplate/{type: [a-zA-Z]*}\r\n";
    body += "POST /addAccount\r\n";
    body += "GET /removeAccount/{accountID: 3a-z0-9]*}\r\n";
    body += "GET /accountList\r\n";
    body += "GET /sendRegister/{accountID: [a-z0-9]*}/{status: (true|false)}\r\n";
    body += "GET /registerAllAccounts\r\n";
    body += "POST /sendTextMessage/{accountID: [a-z0-9]*}/{to: [a-z0-9]*}\r\n";
    body += "GET /messageStatus/{id: [0-9]*}\r\n";
    body += "GET /tlsDefaultSettings\r\n";
    body += "GET /codecList\r\n";
    body += "GET /supportedTlsMethod\r\n";
    body += "GET /supportedCiphers/{accountID: [a-z0-9]*}\r\n";
    body += "GET /codecDetails/{accountID: [a-z0-9]*}/{codecID: [0-9]*}\r\n";
    body += "POST /setCodecDetails/{accountID: [a-z0-9]*}/{codecID: [0-9]*}\r\n";
    body += "GET /activeCodecList/{accountID: [a-z0-9]*}\r\n";
    body += "GET /audioPluginList\r\n";
    body += "GET /setAudioPlugin/{plugin: [a-zA-Z0-9]*}\r\n";
    body += "GET /audioOutputDeviceList\r\n";
    body += "GET /setAudioOutputDevice/{index: [0-9]*}\r\n";
    body += "GET /setAudioInputDevice/{index: [0-9]*}\r\n";
    body += "GET /setAudioRingtoneDevice/{index: [0-9]*}\r\n";
    body += "GET /audioInputDeviceList\r\n";
    body += "GET /currentAudioDeviceIndex\r\n";
    body += "GET /audioInputDeviceIndex/{name: [a-zA-Z0-9]*}\r\n";
    body += "GET /audioOutputDeviceIndex/{name: [a-zA-Z0-9]*}\r\n";
    body += "GET /currentAudioOutputPlugin\r\n";
    body += "GET /noiseSuppressState\r\n";
    body += "GET /setNoiseSuppressState/{state: (true|false)}\r\n";
    body += "GET /isAgcEnable\r\n";
    body += "GET /setAgcState/{state: (true|false)}\r\n";
    body += "GET /muteDtmf/{state: (true|false)}\r\n";
    body += "GET /isDtmfMuted\r\n";
    body += "GET /isCaptureMuted\r\n";
    body += "GET /muteCapture/{state: (true|false)}\r\n";
    body += "GET /isPlaybackMuted\r\n";
    body += "GET /mutePlayback/{state: (true|false)}\r\n";
    body += "GET /isRingtoneMuted\r\n";
    body += "GET /muteRingtone/{state: (true|false)}\r\n";
    body += "GET /audioManager\r\n";
    body += "GET /setAudioManager/{api: [a-zA-Z0-9]*}\r\n";
    body += "GET /supportedAudioManager\r\n";
    body += "GET /isIax2Enable\r\n";
    body += "GET /recordPath\r\n";
    body += "GET /setRecordPath/{path: (\\/[a-zA-Z0-9\\.]{1,}){1,}([a-zA-Z0-9]*\\.[a-zA-Z]*)}\r\n";
    body += "GET /isAlwaysRecording\r\n";
    body += "GET /setIsAlwaysRecording/{status: (true|false)}\r\n";
    body += "GET /setHistoryLimit/{limit: [0-9]*}\r\n";
    body += "GET /getHistoryLimit\r\n";
    body += "POST setAccountsOrder\r\n";
    body += "GET hookSettings\r\n";
    body += "POST setHookSettings\r\n";
    body += "GET credentials/{accountID: [a-z0-9]*}\r\n";
    body += "POST setCredentials/{accountID: [a-z0-9]*}\r\n";
    body += "GET addrFromInterfaceName/{interface: [a-zA-Z0-9]}\r\n";
    body += "GET allIpInterface\r\n";
    body += "GET allIpInterfaceByName\r\n";
    body += "GET shortcuts\r\n";
    body += "POST setShortcuts\r\n";

    const std::multimap<std::string, std::string> headers
    {
        {"Content-Type", "text/html"},
        {"Content-Length", std::to_string(body.length())}
    };

    session->close(restbed::OK, body, headers);
}

void
RestConfigurationManager::getAccountDetails(const std::shared_ptr<restbed::Session> session)
{
    const auto request = session->get_request();
    const std::string accountID = request->get_path_parameter("accountID");

    RING_INFO("[%s] GET /accountDetails/%s", session->get_origin().c_str(), accountID.c_str());

    std::map<std::string, std::string> accountDetails = DRing::getAccountDetails(accountID);

    std::string body = "";

    if(accountDetails.size() == 0)
    {
        session->close(restbed::NOT_FOUND);
    }
    else
    {
        for(auto it = std::begin(accountDetails); it != std::end(accountDetails); ++it)
            body += it->first + " : " + it->second + "\r\n";

        const std::multimap<std::string, std::string> headers
        {
            {"Content-Type", "text/html"},
            {"Content-Length", std::to_string(body.length())}
        };

        session->close(restbed::OK, body, headers);
    }
}

void
RestConfigurationManager::getVolatileAccountDetails(const std::shared_ptr<restbed::Session> session)
{
    const auto request = session->get_request();
    const std::string accountID = request->get_path_parameter("accountID");

    RING_INFO("[%s] GET /volatileAccountDetails/%s", session->get_origin().c_str(), accountID.c_str());

    std::map<std::string, std::string> volatileAccountDetails = DRing::getAccountDetails(accountID);

    std::string body = "";

    if(volatileAccountDetails.size() == 0)
    {
        session->close(restbed::NOT_FOUND);
    }
    else
    {
        for(auto it = std::begin(volatileAccountDetails); it != std::end(volatileAccountDetails); ++it)
            body += it->first + " : " + it->second + "\r\n";

        const std::multimap<std::string, std::string> headers
        {
            {"Content-Type", "text/html"},
            {"Content-Length", std::to_string(body.length())}
        };

        session->close(restbed::OK, body, headers);
    }
}

void
RestConfigurationManager::setAccountDetails(const std::shared_ptr<restbed::Session> session)
{
    const auto request = session->get_request();
    const std::string accountID = request->get_path_parameter("accountID");
    size_t content_length = 0;
    request->get_header("Content-Length", content_length);

    RING_INFO("[%s] POST /setAccountDetails/%s", session->get_origin().c_str(), accountID.c_str());

    session->fetch(content_length, [this, request, accountID](const std::shared_ptr<restbed::Session> session, const restbed::Bytes & body)
    {
        std::string data(std::begin(body), std::end(body));

        std::map<std::string, std::string> details = parsePost(data);
        RING_DBG("Details received");
        for(auto& it : details)
            RING_DBG("%s : %s", it.first.c_str(), it.second.c_str());

        DRing::setAccountDetails(accountID, details);

        session->close(restbed::OK);
    });
}

void
RestConfigurationManager::registerName(const std::shared_ptr<restbed::Session> session)
{
    const auto request = session->get_request();
    const std::string accountID = request->get_path_parameter("accountID");
    size_t content_length = request->get_header("Content-Length", 0);

    RING_INFO("[%s] POST /registerName/%s", session->get_origin().c_str(), accountID.c_str());
    if(content_length > 0){
        session->fetch(content_length, [this, request, accountID](const std::shared_ptr<restbed::Session> session, const restbed::Bytes & body)
        {
            std::string data(std::begin(body), std::end(body));

            std::map<std::string, std::string> details = parsePost(data);
            RING_DBG("Details received");
            for(auto& it : details)
                RING_DBG("%s : %s", it.first.c_str(), it.second.c_str());

            if (details.find("password") == details.end() ){
                session->close(400, "password parameter required");
                return;
            }
            if (details.find("name") == details.end() ){
                session->close(400, "name parameter required");
                return;
            }

            auto response = DRing::registerName(accountID, details["password"], details["name"]);

            session->close(restbed::OK, (response ? "TRUE" : "FALSE"));
        });
    }
    else {
        session->close(400, "empty request");
    }
}

void
RestConfigurationManager::addPendingNameResolutions(const std::string& name, const std::shared_ptr<restbed::Session> session){
    std::lock_guard<std::mutex> lck(pendingNameResolutionMtx);
    this->pendingNameResolutions.insert(std::make_pair(name, session));
}

std::set<std::shared_ptr<restbed::Session>>
RestConfigurationManager::getPendingNameResolutions(const  std::string& name){
    std::lock_guard<std::mutex> lck(pendingNameResolutionMtx);

    auto result = this->pendingNameResolutions.equal_range(name);
    std::set<std::shared_ptr<restbed::Session>> resultSet;
    for (auto it = result.first; it != result.second; ++it){
        resultSet.insert(it->second);
    }
    this->pendingNameResolutions.erase(result.first, result.second);

    return resultSet;
}

void
RestConfigurationManager::lookupName(const std::shared_ptr<restbed::Session> session)
{
    const auto request = session->get_request();
    const std::string name = request->get_path_parameter("name");

    RING_WARN("[%s] GET /lookupName/%s", session->get_origin().c_str(), name.c_str());

    addPendingNameResolutions(name, session);
    DRing::lookupName("", "", name);
}

void
RestConfigurationManager::setAccountActive(const std::shared_ptr<restbed::Session> session)
{
    const auto request = session->get_request();
    auto path = request->get_path_parameters();

    const std::string accountID = request->get_path_parameter("accountID");
    const bool active = (request->get_path_parameter("status") == "true" ? true : false);

    RING_INFO("[%s] GET /setAccountActive/%s/%s", session->get_origin().c_str(), accountID.c_str(), (active ? "true" : "false"));

    DRing::setAccountActive(accountID, active);

    session->close(restbed::OK);
}

void
RestConfigurationManager::getAccountTemplate(const std::shared_ptr<restbed::Session> session)
{
    const auto request = session->get_request();
    const std::string accountType = request->get_path_parameter("type");

    RING_INFO("[%s] GET /accountTemplate/%s", session->get_origin().c_str(), accountType.c_str());

    std::map<std::string, std::string> accountTemplate = DRing::getAccountTemplate(accountType);;

    std::string body = "";

    for(auto it = std::begin(accountTemplate); it != std::end(accountTemplate); ++it)
        body += it->first + " : " + it->second + "\r\n";

    const std::multimap<std::string, std::string> headers
    {
        {"Content-Type", "text/html"},
        {"Content-Length", std::to_string(body.length())}
    };

    session->close(restbed::OK, body, headers);
}

void
RestConfigurationManager::addAccount(const std::shared_ptr<restbed::Session> session)
{
    const auto request = session->get_request();
    size_t content_length = 0;
    request->get_header("Content-Length", content_length);

    RING_INFO("[%s] POST /addAccount", session->get_origin().c_str());

    session->fetch(content_length, [this, request](const std::shared_ptr<restbed::Session> session, const restbed::Bytes & body)
    {
        std::string data(std::begin(body), std::end(body));

        std::map<std::string, std::string> details = parsePost(data);
        RING_DBG("Details received");
        for(auto& it : details)
            RING_DBG("%s : %s", it.first.c_str(), it.second.c_str());

        DRing::addAccount(details);

        session->close(restbed::OK);
    });
}

void
RestConfigurationManager::removeAccount(const std::shared_ptr<restbed::Session> session)
{
    const auto request = session->get_request();
    const std::string accountID = request->get_path_parameter("accountID");

    RING_INFO("[%s] GET /removeAccount/%s", session->get_origin().c_str(), accountID.c_str());

    DRing::removeAccount(accountID);

    // TODO : found a way to know if there's no accound with this id, and send a 404 NOT FOUND
    // See account_factory.cpp:102 for the function
    session->close(restbed::OK);
}

void
RestConfigurationManager::getAccountList(const std::shared_ptr<restbed::Session> session)
{
    RING_INFO("[%s] GET /accountList", session->get_origin().c_str());

    std::vector<std::string> accountList = DRing::getAccountList();

    std::string body = "";

    for(auto& it : accountList)
    {
        body += "accountID : ";
        body += it;
        body += '\n';
    }

    const std::multimap<std::string, std::string> headers
    {
        {"Content-Type", "text/html"},
        {"Content-Length", std::to_string(body.length())}
    };

    session->close(restbed::OK, body, headers);
}

void
RestConfigurationManager::sendRegister(const std::shared_ptr<restbed::Session> session)
{
    const auto request = session->get_request();
    auto path = request->get_path_parameters();

    const std::string accountID = request->get_path_parameter("accountID");
    const bool enable = (request->get_path_parameter("status") == "true" ? true : false);

    RING_INFO("[%s] GET /sendRegister/%s/%s", session->get_origin().c_str(), accountID.c_str(), (enable ? "true" : "false"));

    DRing::sendRegister(accountID, enable);

    session->close(restbed::OK);
}

void
RestConfigurationManager::registerAllAccounts(const std::shared_ptr<restbed::Session> session)
{
    RING_INFO("[%s] GET /registerAllAccounts", session->get_origin().c_str());

    DRing::registerAllAccounts();

    session->close(restbed::OK);
}

void
RestConfigurationManager::sendTextMessage(const std::shared_ptr<restbed::Session> session)
{
    const auto request = session->get_request();
    const std::string accountID = request->get_path_parameter("accountID");
    const std::string to = request->get_path_parameter("to");

    size_t content_length = 0;
    request->get_header("Content-Length", content_length);

    RING_INFO("[%s] POST /sendTextMessage/%s/%s", session->get_origin().c_str(), accountID.c_str(), to.c_str());

    session->fetch(content_length, [this, request, accountID, to](const std::shared_ptr<restbed::Session> session, const restbed::Bytes & body)
    {
        std::string data(std::begin(body), std::end(body));
        std::map<std::string, std::string> payloads = parsePost(data);
        RING_DBG("Payloads received");
        for(auto& it : payloads)
            RING_DBG("%s : %s", it.first.c_str(), it.second.c_str());

        DRing::sendAccountTextMessage(accountID, to, payloads);

        session->close(restbed::OK);
    });
}

void
RestConfigurationManager::getMessageStatus(const std::shared_ptr<restbed::Session> session)
{
    const auto request = session->get_request();
    const std::string id = request->get_path_parameter("id");

    RING_INFO("[%s] GET /messageStatus/%s", session->get_origin().c_str(), id.c_str());

    const std::uint64_t status = DRing::getMessageStatus(std::stoull(id));

    std::string body = "";

    if (status != static_cast<int>(ring::im::MessageStatus::UNKNOWN)) {
        switch (status) {
            case static_cast<int>(ring::im::MessageStatus::IDLE):
            case static_cast<int>(ring::im::MessageStatus::SENDING):
                body = "SENDING";
                break;
            case static_cast<int>(ring::im::MessageStatus::SENT):
                body = "SENT";
                break;
            case static_cast<int>(ring::im::MessageStatus::READ):
                body = "READ";
                break;
            case static_cast<int>(ring::im::MessageStatus::FAILURE):
                body = "FAILURE";
                break;
            default:
                body = "UNKNOWN";
                break;
        }
    }
    else
    {
        body = "UNKNOWN";
    }


    const std::multimap<std::string, std::string> headers
    {
        {"Content-Type", "text/html"},
        {"Content-Length", std::to_string(body.length())}
    };

    session->close(restbed::OK, body, headers);
}

void
RestConfigurationManager::getTlsDefaultSettings(const std::shared_ptr<restbed::Session> session)
{
    RING_INFO("[%s] GET /tlsDefaultSettings", session->get_origin().c_str());

    std::map<std::string, std::string> tlsDefault = DRing::getTlsDefaultSettings();

    std::string body = "";

    for(auto it = std::begin(tlsDefault); it != std::end(tlsDefault); ++it)
        body += it->first + " : " + it->second + "\r\n";

    const std::multimap<std::string, std::string> headers
    {
        {"Content-Type", "text/html"},
        {"Content-Length", std::to_string(body.length())}
    };

    session->close(restbed::OK, body, headers);
}

void
RestConfigurationManager::getCodecList(const std::shared_ptr<restbed::Session> session)
{
    RING_INFO("[%s] GET /codecList", session->get_origin().c_str());

    std::vector<unsigned> codec = DRing::getCodecList();

    std::string body = "";

    for(auto& it : codec)
        body += std::to_string(it) + "\r\n";

    const std::multimap<std::string, std::string> headers
    {
        {"Content-Type", "text/html"},
        {"Content-Length", std::to_string(body.length())}
    };

    session->close(restbed::OK, body, headers);
}

void
RestConfigurationManager::getSupportedTlsMethod(const std::shared_ptr<restbed::Session> session)
{
    RING_INFO("[%s] GET /supportedTlsMethod", session->get_origin().c_str());

    std::vector<std::string> supported = DRing::getSupportedTlsMethod();

    std::string body = "";

    for(auto& it : supported)
        body += it + "\r\n";

    const std::multimap<std::string, std::string> headers
    {
        {"Content-Type", "text/html"},
        {"Content-Length", std::to_string(body.length())}
    };

    session->close(restbed::OK, body, headers);
}

void
RestConfigurationManager::getSupportedCiphers(const std::shared_ptr<restbed::Session> session)
{
    const auto request = session->get_request();
    const std::string accountID = request->get_path_parameter("accountID");

    RING_INFO("[%s] GET /supportedCiphers/%s", session->get_origin().c_str(), accountID.c_str());

    std::vector<std::string> supported = DRing::getSupportedCiphers(accountID);

    std::string body = "";

    for(auto& it : supported)
        body += it + "\r\n";

    const std::multimap<std::string, std::string> headers
    {
        {"Content-Type", "text/html"},
        {"Content-Length", std::to_string(body.length())}
    };

    session->close(restbed::OK, body, headers);
}

void
RestConfigurationManager::getCodecDetails(const std::shared_ptr<restbed::Session> session)
{
    const auto request = session->get_request();
    const std::string accountID = request->get_path_parameter("accountID");
    const std::string codecID = request->get_path_parameter("codecID");

    RING_INFO("[%s] GET /codecDetails/%s/%s", session->get_origin().c_str(), accountID.c_str(), codecID.c_str());

    std::map<std::string, std::string> details = DRing::getCodecDetails(accountID, std::stoi(codecID));

    std::string body = "";

    for(auto& it : details)
        body += it.first + " : " + it.second + "\r\n";

    const std::multimap<std::string, std::string> headers
    {
        {"Content-Type", "text/html"},
        {"Content-Length", std::to_string(body.length())}
    };

    session->close(restbed::OK, body, headers);
}

void
RestConfigurationManager::setCodecDetails(const std::shared_ptr<restbed::Session> session)
{
    const auto request = session->get_request();
    const std::string accountID = request->get_path_parameter("accountID");
    const std::string codecID = request->get_path_parameter("codecID");

    size_t content_length = 0;
    request->get_header("Content-Length", content_length);

    RING_INFO("[%s] POST /setCodecDetails/%s/%s", session->get_origin().c_str(), accountID.c_str(), codecID.c_str());

    session->fetch(content_length, [this, request, accountID, codecID](const std::shared_ptr<restbed::Session> session, const restbed::Bytes & body)
    {
        std::string data(std::begin(body), std::end(body));
        std::map<std::string, std::string> details = parsePost(data);

        RING_DBG("Details received");
        for(auto& it : details)
            RING_DBG("%s : %s", it.first.c_str(), it.second.c_str());

        DRing::setCodecDetails(accountID, std::stoi(codecID), details);

        session->close(restbed::OK);
    });
}

void
RestConfigurationManager::getActiveCodecList(const std::shared_ptr<restbed::Session> session)
{
    const auto request = session->get_request();
    const std::string accountID = request->get_path_parameter("accountID");

    RING_INFO("[%s] GET /activeCodecList/%s", session->get_origin().c_str(), accountID.c_str());

    std::vector<unsigned> codecs = DRing::getActiveCodecList(accountID);

    std::string body = "";

    for(auto& it : codecs)
        body += std::to_string(it) + "\r\n";

    const std::multimap<std::string, std::string> headers
    {
        {"Content-Type", "text/html"},
        {"Content-Length", std::to_string(body.length())}
    };

    session->close(restbed::OK, body, headers);
}

void
RestConfigurationManager::setActiveCodecList(const std::string& accountID, const std::vector<unsigned>& list)
{
    // See restconfigurationmanager.h:70

    //DRing::setActiveCodecList(accountID, list);
}

void
RestConfigurationManager::getAudioPluginList(const std::shared_ptr<restbed::Session> session)
{
    RING_INFO("[%s] GET /audioPluginList", session->get_origin().c_str());

    std::vector<std::string> list = DRing::getAudioPluginList();

    std::string body = "";

    for(auto& it : list)
        body += it + "\r\n";

    const std::multimap<std::string, std::string> headers
    {
        {"Content-Type", "text/html"},
        {"Content-Length", std::to_string(body.length())}
    };

    session->close(restbed::OK, body, headers);
}

void
RestConfigurationManager::setAudioPlugin(const std::shared_ptr<restbed::Session> session)
{
    const auto request = session->get_request();
    const std::string plugin = request->get_path_parameter("plugin");

    RING_INFO("[%s] GET /setAudioPlugin/%s", session->get_origin().c_str(), plugin.c_str());

    DRing::setAudioPlugin(plugin);

    session->close(restbed::OK);
}

void
RestConfigurationManager::getAudioOutputDeviceList(const std::shared_ptr<restbed::Session> session)
{
    RING_INFO("[%s] GET /audioOutputDeviceList", session->get_origin().c_str());

    std::vector<std::string> list = DRing::getAudioOutputDeviceList();

    std::string body = "";

    for(auto& it : list)
        body += it + "\r\n";

    const std::multimap<std::string, std::string> headers
    {
        {"Content-Type", "text/html"},
        {"Content-Length", std::to_string(body.length())}
    };

    session->close(restbed::OK, body, headers);
}

void
RestConfigurationManager::setAudioOutputDevice(const std::shared_ptr<restbed::Session> session)
{
    const auto request = session->get_request();
    const std::string index = request->get_path_parameter("index");

    RING_INFO("[%s] GET /setAudioOutputDevice/%s", session->get_origin().c_str(), index.c_str());

    DRing::setAudioOutputDevice(std::stoi(index));

    session->close(restbed::OK);
}

void
RestConfigurationManager::setAudioInputDevice(const std::shared_ptr<restbed::Session> session)
{
    const auto request = session->get_request();
    const std::string index = request->get_path_parameter("index");

    RING_INFO("[%s] GET /setAudioInputDevice/%s", session->get_origin().c_str(), index.c_str());

    DRing::setAudioInputDevice(std::stoi(index));

    session->close(restbed::OK);
}

void
RestConfigurationManager::setAudioRingtoneDevice(const std::shared_ptr<restbed::Session> session)
{
    const auto request = session->get_request();
    const std::string index = request->get_path_parameter("index");

    RING_INFO("[%s] GET /setAudioRingtoneDevice/%s", session->get_origin().c_str(), index.c_str());

    DRing::setAudioRingtoneDevice(std::stoi(index));

    session->close(restbed::OK);
}

void
RestConfigurationManager::getAudioInputDeviceList(const std::shared_ptr<restbed::Session> session)
{
    RING_INFO("[%s] GET /audioInputDeviceList", session->get_origin().c_str());

    std::vector<std::string> list = DRing::getAudioInputDeviceList();

    std::string body = "";

    for(auto& it : list)
        body += it + "\r\n";

    const std::multimap<std::string, std::string> headers
    {
        {"Content-Type", "text/html"},
        {"Content-Length", std::to_string(body.length())}
    };

    session->close(restbed::OK, body, headers);
}

void
RestConfigurationManager::getCurrentAudioDevicesIndex(const std::shared_ptr<restbed::Session> session)
{
    RING_INFO("[%s] GET /currentAudioDevicesIndex", session->get_origin().c_str());

    std::vector<std::string> list = DRing::getCurrentAudioDevicesIndex();

    std::string body = "";

    for(auto& it : list)
        body += it + "\r\n";

    const std::multimap<std::string, std::string> headers
    {
        {"Content-Type", "text/html"},
        {"Content-Length", std::to_string(body.length())}
    };

    session->close(restbed::OK, body, headers);
}

void
RestConfigurationManager::getAudioInputDeviceIndex(const std::shared_ptr<restbed::Session> session)
{
    const auto request = session->get_request();
    const std::string name = request->get_path_parameter("name");

    RING_INFO("[%s] GET /audioInputDeviceIndex/%s", session->get_origin().c_str(), name.c_str());

    std::int32_t index = DRing::getAudioInputDeviceIndex(name);

    std::string body = std::to_string(index) + "\r\n";

    const std::multimap<std::string, std::string> headers
    {
        {"Content-Type", "text/html"},
        {"Content-Length", std::to_string(body.length())}
    };

    session->close(restbed::OK, body, headers);
}

void
RestConfigurationManager::getAudioOutputDeviceIndex(const std::shared_ptr<restbed::Session> session)
{
    const auto request = session->get_request();
    const std::string name = request->get_path_parameter("name");

    RING_INFO("[%s] GET /audioOutputDeviceIndex/%s", session->get_origin().c_str(), name.c_str());

    std::int32_t index = DRing::getAudioOutputDeviceIndex(name);

    std::string body = std::to_string(index) + "\r\n";

    const std::multimap<std::string, std::string> headers
    {
        {"Content-Type", "text/html"},
        {"Content-Length", std::to_string(body.length())}
    };

    session->close(restbed::OK, body, headers);
}

void
RestConfigurationManager::getCurrentAudioOutputPlugin(const std::shared_ptr<restbed::Session> session)
{
    RING_INFO("[%s] GET /currentAudioOutputPlugin", session->get_origin().c_str());

    std::string body = DRing::getCurrentAudioOutputPlugin();

    const std::multimap<std::string, std::string> headers
    {
        {"Content-Type", "text/html"},
        {"Content-Length", std::to_string(body.length())}
    };

    session->close(restbed::OK, body, headers);
}

void
RestConfigurationManager::getNoiseSuppressState(const std::shared_ptr<restbed::Session> session)
{
    RING_INFO("[%s] GET /noiseSuppressState", session->get_origin().c_str());

    std::string body = (DRing::getNoiseSuppressState() ? "true" : "false");

    const std::multimap<std::string, std::string> headers
    {
        {"Content-Type", "text/html"},
        {"Content-Length", std::to_string(body.length())}
    };

    session->close(restbed::OK, body, headers);
}

void
RestConfigurationManager::setNoiseSuppressState(const std::shared_ptr<restbed::Session> session)
{
    const auto request = session->get_request();
    const std::string state = request->get_path_parameter("state");

    RING_INFO("[%s] GET /setNoiseSuppressState/%s", session->get_origin().c_str(), state.c_str());

    DRing::setNoiseSuppressState((state == "true" ? true : false));

    session->close(restbed::OK);
}

void
RestConfigurationManager::isAgcEnabled(const std::shared_ptr<restbed::Session> session)
{
    RING_INFO("[%s] GET /isAgcEnable", session->get_origin().c_str());

    bool status = DRing::isAgcEnabled();
    std::string body = (status ? "true" : "false");

    const std::multimap<std::string, std::string> headers
    {
        {"Content-Type", "text/html"},
        {"Content-Length", std::to_string(body.length())}
    };

    session->close(restbed::OK, body, headers);
}

void
RestConfigurationManager::setAgcState(const std::shared_ptr<restbed::Session> session)
{
    const auto request = session->get_request();
    const std::string state = request->get_path_parameter("state");

    RING_INFO("[%s] GET /setAgcState/%s", session->get_origin().c_str(), state.c_str());

    DRing::setAgcState((state == "true" ? true : false));

    session->close(restbed::OK);
}

void
RestConfigurationManager::muteDtmf(const std::shared_ptr<restbed::Session> session)
{
    const auto request = session->get_request();
    const std::string state = request->get_path_parameter("state");

    RING_INFO("[%s] GET /muteDtmf/%s", session->get_origin().c_str(), state.c_str());

    DRing::muteDtmf((state == "true" ? true : false));

    session->close(restbed::OK);
}

void
RestConfigurationManager::isDtmfMuted(const std::shared_ptr<restbed::Session> session)
{
    RING_INFO("[%s] GET /isDtmfMuted", session->get_origin().c_str());

    bool status = DRing::isDtmfMuted();
    std::string body = (status ? "true" : "false");

    const std::multimap<std::string, std::string> headers
    {
        {"Content-Type", "text/html"},
        {"Content-Length", std::to_string(body.length())}
    };

    session->close(restbed::OK, body, headers);
}

void
RestConfigurationManager::isCaptureMuted(const std::shared_ptr<restbed::Session> session)
{
    RING_INFO("[%s] GET /isCaptureMuted", session->get_origin().c_str());

    bool status = DRing::isCaptureMuted();
    std::string body = (status ? "true" : "false");

    const std::multimap<std::string, std::string> headers
    {
        {"Content-Type", "text/html"},
        {"Content-Length", std::to_string(body.length())}
    };

    session->close(restbed::OK, body, headers);
}

void
RestConfigurationManager::muteCapture(const std::shared_ptr<restbed::Session> session)
{
    const auto request = session->get_request();
    const std::string state = request->get_path_parameter("state");

    RING_INFO("[%s] GET /muteCapture/%s", session->get_origin().c_str(), state.c_str());

    DRing::muteCapture((state == "true" ? true : false));

    session->close(restbed::OK);
}

void
RestConfigurationManager::isPlaybackMuted(const std::shared_ptr<restbed::Session> session)
{
    RING_INFO("[%s] GET /isPlaybackMuted", session->get_origin().c_str());

    bool status = DRing::isPlaybackMuted();
    std::string body = (status ? "true" : "false");

    const std::multimap<std::string, std::string> headers
    {
        {"Content-Type", "text/html"},
        {"Content-Length", std::to_string(body.length())}
    };

    session->close(restbed::OK, body, headers);
}

void
RestConfigurationManager::mutePlayback(const std::shared_ptr<restbed::Session> session)
{
    const auto request = session->get_request();
    const std::string state = request->get_path_parameter("state");

    RING_INFO("[%s] GET /mutePlayback/%s", session->get_origin().c_str(), state.c_str());

    DRing::mutePlayback((state == "true" ? true : false));

    session->close(restbed::OK);
}

void
RestConfigurationManager::isRingtoneMuted(const std::shared_ptr<restbed::Session> session)
{
    RING_INFO("[%s] GET /isRingtoneMuted", session->get_origin().c_str());

    bool status = DRing::isRingtoneMuted();
    std::string body = (status ? "true" : "false");

    const std::multimap<std::string, std::string> headers
    {
        {"Content-Type", "text/html"},
        {"Content-Length", std::to_string(body.length())}
    };

    session->close(restbed::OK, body, headers);
}

void
RestConfigurationManager::muteRingtone(const std::shared_ptr<restbed::Session> session)
{
    const auto request = session->get_request();
    const std::string state = request->get_path_parameter("state");

    RING_INFO("[%s] GET /muteRingtone/%s", session->get_origin().c_str(), state.c_str());

    DRing::muteRingtone((state == "true" ? true : false));

    session->close(restbed::OK);
}

void
RestConfigurationManager::getAudioManager(const std::shared_ptr<restbed::Session> session)
{
    RING_INFO("[%s] GET /audioManager", session->get_origin().c_str());

    std::string body = DRing::getAudioManager();

    const std::multimap<std::string, std::string> headers
    {
        {"Content-Type", "text/html"},
        {"Content-Length", std::to_string(body.length())}
    };

    session->close(restbed::OK, body, headers);
}

void
RestConfigurationManager::setAudioManager(const std::shared_ptr<restbed::Session> session)
{
    const auto request = session->get_request();
    const std::string api = request->get_path_parameter("api");

    RING_INFO("[%s] GET /setAudioManager/%s", session->get_origin().c_str(), api.c_str());

    bool status = DRing::setAudioManager(api);
    std::string body = (status ? "true" : "false");

    const std::multimap<std::string, std::string> headers
    {
        {"Content-Type", "text/html"},
        {"Content-Length", std::to_string(body.length())}
    };

    session->close(restbed::OK, body, headers);
}

void
RestConfigurationManager::getSupportedAudioManagers(const std::shared_ptr<restbed::Session> session)
{
    const auto request = session->get_request();

    RING_INFO("[%s] GET /supportedAudioManager", session->get_origin().c_str());

    std::string body = "";
#if HAVE_ALSA
    body += ALSA_API_STR + "\r\n",
#endif
#if HAVE_PULSE
    body += PULSEAUDIO_API_STR + "\r\n",
#endif
#if HAVE_JACK
    body += JACK_API_STR + "\r\n",
#endif

    const std::multimap<std::string, std::string> headers
    {
        {"Content-Type", "text/html"},
        {"Content-Length", std::to_string(body.length())}
    };

    session->close(restbed::OK, body, headers);
}

void
RestConfigurationManager::getRecordPath(const std::shared_ptr<restbed::Session> session)
{
    RING_INFO("[%s] GET /recordPath", session->get_origin().c_str());

    std::string body = DRing::getRecordPath();

    const std::multimap<std::string, std::string> headers
    {
        {"Content-Type", "text/html"},
        {"Content-Length", std::to_string(body.length())}
    };

    session->close(restbed::OK, body, headers);
}

void
RestConfigurationManager::setRecordPath(const std::shared_ptr<restbed::Session> session)
{
    const auto request = session->get_request();
    const std::string path = request->get_path_parameter("path");

    RING_INFO("[%s] GET /setRecordPath/%s", session->get_origin().c_str(), path.c_str());

    DRing::setRecordPath(path);

    session->close(restbed::OK);
}

void
RestConfigurationManager::getIsAlwaysRecording(const std::shared_ptr<restbed::Session> session)
{
    RING_INFO("[%s] GET /isAlwaysRecording", session->get_origin().c_str());

    bool status = DRing::getIsAlwaysRecording();
    std::string body = (status ? "true" : "false");

    const std::multimap<std::string, std::string> headers
    {
        {"Content-Type", "text/html"},
        {"Content-Length", std::to_string(body.length())}
    };

    session->close(restbed::OK, body, headers);
}

void
RestConfigurationManager::setIsAlwaysRecording(const std::shared_ptr<restbed::Session> session)
{
    const auto request = session->get_request();
    const std::string state = request->get_path_parameter("state");

    RING_INFO("[%s] GET /setIsAlwaysRecording/%s", session->get_origin().c_str(), state.c_str());

    DRing::setIsAlwaysRecording((state == "true" ? true : false));

    session->close(restbed::OK);
}

void
RestConfigurationManager::setHistoryLimit(const std::shared_ptr<restbed::Session> session)
{
    const auto request = session->get_request();
    const std::string days = request->get_path_parameter("limit");

    RING_INFO("[%s] GET /setHistoryLimit/%s", session->get_origin().c_str(), days.c_str());

    DRing::setHistoryLimit(std::stoi(days));

    session->close(restbed::OK);
}

void
RestConfigurationManager::getHistoryLimit(const std::shared_ptr<restbed::Session> session)
{
    RING_INFO("[%s] GET /getHistoryLimit", session->get_origin().c_str());

    std::string body = std::to_string(DRing::getHistoryLimit());

    const std::multimap<std::string, std::string> headers
    {
        {"Content-Type", "text/html"},
        {"Content-Length", std::to_string(body.length())}
    };

    session->close(restbed::OK, body, headers);
}

void
RestConfigurationManager::setAccountsOrder(const std::shared_ptr<restbed::Session> session)
{
    // POST order=accountID/accountID/ (etc)

    const auto request = session->get_request();

    size_t content_length = 0;
    request->get_header("Content-Length", content_length);

    RING_INFO("[%s] POST /setAccountsOrder", session->get_origin().c_str());

    session->fetch(content_length, [this](const std::shared_ptr<restbed::Session> session, const restbed::Bytes & body)
    {
        std::string data(std::begin(body), std::end(body));

        std::map<std::string, std::string> details = parsePost(data);
        RING_DBG("Order received");
        for(auto& it : details)
            RING_DBG("%s : %s", it.first.c_str(), it.second.c_str());

        std::regex order("[a-z0-9]{16}\\/");

        auto search = details.find("order");
        if(search != details.end() && std::regex_match(details["order"], order))
            DRing::setAccountsOrder(details["order"]);

        session->close(restbed::OK);
    });
}

void
RestConfigurationManager::getHookSettings(const std::shared_ptr<restbed::Session> session)
{
    RING_INFO("[%s] GET /hookSettings", session->get_origin().c_str());

    std::map<std::string, std::string> hooks = DRing::getHookSettings();

    std::string body = "";

    for(auto& it : hooks)
        body += it.first + " : " + it.second + "\r\n";

    const std::multimap<std::string, std::string> headers
    {
        {"Content-Type", "text/html"},
        {"Content-Length", std::to_string(body.length())}
    };

    session->close(restbed::OK, body, headers);
}

void
RestConfigurationManager::setHookSettings(const std::shared_ptr<restbed::Session> session)
{
    const auto request = session->get_request();

    size_t content_length = 0;
    request->get_header("Content-Length", content_length);

    RING_INFO("[%s] POST /setHookSettings", session->get_origin().c_str());

    session->fetch(content_length, [this](const std::shared_ptr<restbed::Session> session, const restbed::Bytes & body)
    {
        std::string data(std::begin(body), std::end(body));

        std::map<std::string, std::string> settings = parsePost(data);
        RING_DBG("Settings received");
        for(auto& it : settings)
            RING_DBG("%s : %s", it.first.c_str(), it.second.c_str());

        DRing::setHookSettings(settings);

        session->close(restbed::OK);
    });
}

void
RestConfigurationManager::getCredentials(const std::shared_ptr<restbed::Session> session)
{
    const auto request = session->get_request();
    const std::string accountID = request->get_path_parameter("accountID");

    RING_INFO("[%s] GET /credentials/%s", session->get_origin().c_str(), accountID.c_str());

    std::vector<std::map<std::string, std::string>> credentials = DRing::getCredentials(accountID);

    std::string body = "";

    for(auto& it : credentials)
        for(auto& i : it)
            body += i.first + " : " + i.second + "\r\n";

    const std::multimap<std::string, std::string> headers
    {
        {"Content-Type", "text/html"},
        {"Content-Length", std::to_string(body.length())}
    };

    session->close(restbed::OK, body, headers);
}

void
RestConfigurationManager::setCredentials(const std::shared_ptr<restbed::Session> session)
{
    const auto request = session->get_request();
    const std::string accountID = request->get_path_parameter("accountID");

    RING_INFO("[%s] POST /setCredentials/%s", session->get_origin().c_str(), accountID.c_str());

    size_t content_length = 0;
    request->get_header("Content-Length", content_length);

    session->fetch(content_length, [this, accountID](const std::shared_ptr<restbed::Session> session, const restbed::Bytes & body)
    {
        std::string data(std::begin(body), std::end(body));

        std::map<std::string, std::string> details = parsePost(data);
        RING_DBG("Details received");
        for(auto& it : details)
            RING_DBG("%s : %s", it.first.c_str(), it.second.c_str());

        DRing::setCredentials(accountID, std::vector<std::map<std::string, std::string>>{details});

        session->close(restbed::OK);
    });
}

void
RestConfigurationManager::getAddrFromInterfaceName(const std::shared_ptr<restbed::Session> session)
{
    const auto request = session->get_request();
    const std::string interface = request->get_path_parameter("interface");

    RING_INFO("[%s] GET /addrFromInterfaceName/%s", session->get_origin().c_str(), interface.c_str());

    std::string body = DRing::getAddrFromInterfaceName(interface);

    const std::multimap<std::string, std::string> headers
    {
        {"Content-Type", "text/html"},
        {"Content-Length", std::to_string(body.length())}
    };

    session->close(restbed::OK, body, headers);
}

void
RestConfigurationManager::getAllIpInterface(const std::shared_ptr<restbed::Session> session)
{
    RING_INFO("[%s] GET /allIpInterface", session->get_origin().c_str());

    std::vector<std::string> interfaces = DRing::getAllIpInterface();

    std::string body = "";

    for(auto& it : interfaces)
        body += it + "\r\n";

    const std::multimap<std::string, std::string> headers
    {
        {"Content-Type", "text/html"},
        {"Content-Length", std::to_string(body.length())}
    };

    session->close(restbed::OK, body, headers);
}

void
RestConfigurationManager::getAllIpInterfaceByName(const std::shared_ptr<restbed::Session> session)
{
    RING_INFO("[%s] GET /allIpInterfaceByName", session->get_origin().c_str());

    std::vector<std::string> interfaces = DRing::getAllIpInterfaceByName();

    std::string body = "";

    for(auto& it : interfaces)
        body += it + "\r\n";

    const std::multimap<std::string, std::string> headers
    {
        {"Content-Type", "text/html"},
        {"Content-Length", std::to_string(body.length())}
    };

    session->close(restbed::OK, body, headers);
}

void
RestConfigurationManager::getShortcuts(const std::shared_ptr<restbed::Session> session)
{
    RING_INFO("[%s] GET /shortcuts", session->get_origin().c_str());

    std::map<std::string, std::string> shortcuts = DRing::getShortcuts();

    std::string body = "";

    for(auto& it : shortcuts)
        body += it.first + " : " + it.second + "\r\n";

    const std::multimap<std::string, std::string> headers
    {
        {"Content-Type", "text/html"},
        {"Content-Length", std::to_string(body.length())}
    };

    session->close(restbed::OK, body, headers);
}

void
RestConfigurationManager::setShortcuts(const std::shared_ptr<restbed::Session> session)
{
    const auto request = session->get_request();

    RING_INFO("[%s] POST /setShortcuts", session->get_origin().c_str());

    size_t content_length = 0;
    request->get_header("Content-Length", content_length);

    session->fetch(content_length, [this](const std::shared_ptr<restbed::Session> session, const restbed::Bytes & body)
    {
        std::string data(std::begin(body), std::end(body));

        std::map<std::string, std::string> shortcutsMap = parsePost(data);
        RING_DBG("shortcutsMap received");
        for(auto& it : shortcutsMap)
            RING_DBG("%s : %s", it.first.c_str(), it.second.c_str());

        DRing::setShortcuts(shortcutsMap);

        session->close(restbed::OK);
    });
}

void
RestConfigurationManager::setVolume(const std::string& device, const double& value)
{
    DRing::setVolume(device, value);
}

void
RestConfigurationManager::getVolume(const std::string& device)
{
    //return DRing::getVolume(device);
}

void
RestConfigurationManager::validateCertificate(const std::string& accountId, const std::string& certificate)
{
   //return DRing::validateCertificate(accountId, certificate);
}

void
RestConfigurationManager::validateCertificatePath(const std::string& accountId, const std::string& certificate, const std::string& privateKey, const std::string& privateKeyPass, const std::string& caList)
{
   //return DRing::validateCertificatePath(accountId, certificate, privateKey, privateKeyPass, caList);
}

void
RestConfigurationManager::getCertificateDetails(const std::string& certificate)
{
    //return DRing::getCertificateDetails(certificate);
}

void
RestConfigurationManager::getCertificateDetailsPath(const std::string& certificate, const std::string& privateKey, const std::string& privateKeyPass)
{
    //return DRing::getCertificateDetailsPath(certificate, privateKey, privateKeyPass);
}

void
RestConfigurationManager::getPinnedCertificates()
{
    //return DRing::getPinnedCertificates();
}

void
RestConfigurationManager::pinCertificate(const std::vector<uint8_t>& certificate, const bool& local)
{
    //return DRing::pinCertificate(certificate, local);
}

void
RestConfigurationManager::pinCertificatePath(const std::string& certPath)
{
    //return DRing::pinCertificatePath(certPath);
}

void
RestConfigurationManager::unpinCertificate(const std::string& certId)
{
    //return DRing::unpinCertificate(certId);
}

void
RestConfigurationManager::unpinCertificatePath(const std::string& p)
{
    //return DRing::unpinCertificatePath(p);
}

void
RestConfigurationManager::pinRemoteCertificate(const std::string& accountId, const std::string& certId)
{
    //return DRing::pinRemoteCertificate(accountId, certId);
}

void
RestConfigurationManager::setCertificateStatus(const std::string& accountId, const std::string& certId, const std::string& status)
{
    //return DRing::setCertificateStatus(accountId, certId, status);
}

void
RestConfigurationManager::getCertificatesByStatus(const std::string& accountId, const std::string& status)
{
    //return DRing::getCertificatesByStatus(accountId, status);
}

void
RestConfigurationManager::getTrustRequests(const std::shared_ptr<restbed::Session> session)
{
    //return DRing::getTrustRequests(accountId);
}

void
RestConfigurationManager::acceptTrustRequest(const std::string& accountId, const std::string& from)
{
    //return DRing::acceptTrustRequest(accountId, from);
}

void
RestConfigurationManager::discardTrustRequest(const std::string& accountId, const std::string& from)
{
    //return DRing::discardTrustRequest(accountId, from);
}

void
RestConfigurationManager::sendTrustRequest(const std::string& accountId, const std::string& to, const std::vector<uint8_t>& payload)
{
    DRing::sendTrustRequest(accountId, to, payload);
}

void
RestConfigurationManager::exportAccounts(const std::vector<std::string>& accountIDs, const std::string& filepath, const std::string& password)
{
    //return DRing::exportAccounts(accountIDs, filepath, password);
}

void
RestConfigurationManager::importAccounts(const std::string& archivePath, const std::string& password)
{
    //return DRing::importAccounts(archivePath, password);
}
