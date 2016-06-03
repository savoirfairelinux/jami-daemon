#include "restconfigurationmanager.h"

RestConfigurationManager::RestConfigurationManager() :
    resources_()
{
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
    /*	Template
    resources_.push_back(std::make_shared<restbed::Resource>());
    resources_.back()->set_path("/");
    resources_.back()->set_method_handler("",
        std::bind(&RestConfigurationManager::, this, std::placeholders::_1));
     */

    resources_.push_back(std::make_shared<restbed::Resource>());
    resources_.back()->set_path("/");
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
}

void
RestConfigurationManager::defaultRoute(const std::shared_ptr<restbed::Session> session)
{
    RING_INFO("[%s] GET /", session->get_origin().c_str());

	std::string body = "Available routes are : \r\n";
    body += "GET /accountDetails/{accountID: [a-z0-9]*}\r\n";
    body += "GET /volatileAccountDetails/{accountID: [a-z0-9]*}\r\n";
    body += "POST /setAccountDetails/{accountID: [a-z0-9]*}\r\n";
    body += "GET /setAccountActive/{accountID: [a-z0-9]*}/{status: (true|false)}\r\n";
    body += "GET /accountTemplate/{type: [a-zA-Z]*}\r\n";
    body += "POST /addAccount\r\n";
    body += "GET /removeAccount/{accountID: [a-z0-9]*}\r\n";
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

    RING_INFO("[%s] GET /messageStatus/{id: [0-9]*}", session->get_origin().c_str(), id.c_str());

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

    std::map<std::string, std::string> details = DRing::getCodecDetails(accountID, std::stoi(codecId));

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
RestConfigurationManager::setCodecDetails(const std::string& accountID, const unsigned& codecId, const std::map<std::string, std::string>& details)
{
    //return DRing::setCodecDetails(accountID, codecId, details);
}

void
RestConfigurationManager::getActiveCodecList(const std::shared_ptr<restbed::Session> session)
{
    //return DRing::getActiveCodecList(accountID);
}

void
RestConfigurationManager::setActiveCodecList(const std::string& accountID, const std::vector<unsigned>& list)
{
    DRing::setActiveCodecList(accountID, list);
}

void
RestConfigurationManager::getAudioPluginList()
{
    //return DRing::getAudioPluginList();
}

void
RestConfigurationManager::setAudioPlugin(const std::string& audioPlugin)
{
    DRing::setAudioPlugin(audioPlugin);
}

void
RestConfigurationManager::getAudioOutputDeviceList()
{
    //return DRing::getAudioOutputDeviceList();
}

void
RestConfigurationManager::setAudioOutputDevice(const int32_t& index)
{
    DRing::setAudioOutputDevice(index);
}

void
RestConfigurationManager::setAudioInputDevice(const int32_t& index)
{
    DRing::setAudioInputDevice(index);
}

void
RestConfigurationManager::setAudioRingtoneDevice(const int32_t& index)
{
    DRing::setAudioRingtoneDevice(index);
}

void
RestConfigurationManager::getAudioInputDeviceList()
{
    //return DRing::getAudioInputDeviceList();
}

void
RestConfigurationManager::getCurrentAudioDevicesIndex()
{
    //return DRing::getCurrentAudioDevicesIndex();
}

void
RestConfigurationManager::getAudioInputDeviceIndex(const std::string& name)
{
    //return DRing::getAudioInputDeviceIndex(name);
}

void
RestConfigurationManager::getAudioOutputDeviceIndex(const std::string& name)
{
    //return DRing::getAudioOutputDeviceIndex(name);
}

void
RestConfigurationManager::getCurrentAudioOutputPlugin()
{
    //return DRing::getCurrentAudioOutputPlugin();
}

void
RestConfigurationManager::getNoiseSuppressState()
{
    //return DRing::getNoiseSuppressState();
}

void
RestConfigurationManager::setNoiseSuppressState(const bool& state)
{
    DRing::setNoiseSuppressState(state);
}

void
RestConfigurationManager::isAgcEnabled()
{
    //return DRing::isAgcEnabled();
}

void
RestConfigurationManager::setAgcState(const bool& enabled)
{
    DRing::setAgcState(enabled);
}

void
RestConfigurationManager::muteDtmf(const bool& mute)
{
    DRing::muteDtmf(mute);
}

void
RestConfigurationManager::isDtmfMuted()
{
    //return DRing::isDtmfMuted();
}

void
RestConfigurationManager::isCaptureMuted()
{
    //return DRing::isCaptureMuted();
}

void
RestConfigurationManager::muteCapture(const bool& mute)
{
    DRing::muteCapture(mute);
}

void
RestConfigurationManager::isPlaybackMuted()
{
    //return DRing::isPlaybackMuted();
}

void
RestConfigurationManager::mutePlayback(const bool& mute)
{
    DRing::mutePlayback(mute);
}

void
RestConfigurationManager::isRingtoneMuted()
{
    //return DRing::isRingtoneMuted();
}

void
RestConfigurationManager::muteRingtone(const bool& mute)
{
    DRing::muteRingtone(mute);
}

void
RestConfigurationManager::getAudioManager()
{
    //return DRing::getAudioManager();
}

void
RestConfigurationManager::setAudioManager(const std::string& api)
{
    //return DRing::setAudioManager(api);
}

void
RestConfigurationManager::getSupportedAudioManagers()
{
    /*
    return {
#if HAVE_ALSA
        ALSA_API_STR,
#endif
#if HAVE_PULSE
        PULSEAUDIO_API_STR,
#endif
#if HAVE_JACK
        JACK_API_STR,
#endif
    };
    */
}

void
RestConfigurationManager::isIax2Enabled()
{
    //return DRing::isIax2Enabled();
}

void
RestConfigurationManager::getRecordPath()
{
    //return DRing::getRecordPath();
}

void
RestConfigurationManager::setRecordPath(const std::string& recPath)
{
    DRing::setRecordPath(recPath);
}

void
RestConfigurationManager::getIsAlwaysRecording()
{
    //return DRing::getIsAlwaysRecording();
}

void
RestConfigurationManager::setIsAlwaysRecording(const bool& rec)
{
    DRing::setIsAlwaysRecording(rec);
}

void
RestConfigurationManager::setHistoryLimit(const int32_t& days)
{
    DRing::setHistoryLimit(days);
}

void
RestConfigurationManager::getHistoryLimit()
{
    //return DRing::getHistoryLimit();
}

void
RestConfigurationManager::setAccountsOrder(const std::string& order)
{
    DRing::setAccountsOrder(order);
}

void
RestConfigurationManager::getHookSettings()
{
    //return DRing::getHookSettings();
}

void
RestConfigurationManager::setHookSettings(const std::map<std::string, std::string>& settings)
{
    DRing::setHookSettings(settings);
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
RestConfigurationManager::getCredentials(const std::shared_ptr<restbed::Session> session)
{
    //return DRing::getCredentials(accountID);
}

void
RestConfigurationManager::setCredentials(const std::string& accountID, const std::vector<std::map<std::string, std::string>>& details)
{
    DRing::setCredentials(accountID, details);
}

void
RestConfigurationManager::getAddrFromInterfaceName(const std::string& interface)
{
    //return DRing::getAddrFromInterfaceName(interface);
}

void
RestConfigurationManager::getAllIpInterface()
{
    //return DRing::getAllIpInterface();
}

void
RestConfigurationManager::getAllIpInterfaceByName()
{
    //return DRing::getAllIpInterfaceByName();
}

void
RestConfigurationManager::getShortcuts()
{
    //return DRing::getShortcuts();
}

void
RestConfigurationManager::setShortcuts(const std::map<std::string, std::string> &shortcutsMap)
{
    DRing::setShortcuts(shortcutsMap);
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
RestConfigurationManager::exportAccounts(const std::vector<std::string>& accountIDs, const std::string& filepath, const std::string& password)
{
    //return DRing::exportAccounts(accountIDs, filepath, password);
}

void
RestConfigurationManager::importAccounts(const std::string& archivePath, const std::string& password)
{
    //return DRing::importAccounts(archivePath, password);
}
