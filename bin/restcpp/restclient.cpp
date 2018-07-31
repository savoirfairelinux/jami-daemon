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
#include "restclient.h"

RestClient::RestClient(int port, int flags, bool persistent) :
    service_()
{
    configurationManager_.reset(new RestConfigurationManager());
    videoManager_.reset(new RestVideoManager());

    if (initLib(flags) < 0)
        throw std::runtime_error {"cannot initialize libring"};

    // Fill the resources
    initResources();

    // Initiate the rest service
    settings_ = std::make_shared<restbed::Settings>();
    settings_->set_port(port);
    settings_->set_default_header( "Connection", "close" );
    RING_INFO("Restclient running on port [%d]", port);

    // Make it run in a thread, because this is a blocking function
    restbed = std::thread([this](){
        service_.start(settings_);
    });
}

RestClient::~RestClient()
{
    RING_INFO("destroying RestClient");
    exit();
}

int
RestClient::event_loop() noexcept
{
    // While the client is running, the events are polled every 10 milliseconds
    RING_INFO("Restclient starting to poll events");
    while(!pollNoMore_)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return 0;
}

int
RestClient::exit() noexcept
{
    try {
        // On exit, the client stop polling events
        pollNoMore_ = true;
        // The rest service is stopped
        service_.stop();
        // And the thread running the service is joined
        restbed.join();
        endLib();
    } catch (const std::exception& err) {
        std::cerr << "quitting: " << err.what() << std::endl;
        return 1;
    }

    return 0;
}

int
RestClient::initLib(int flags)
{
    using namespace std::placeholders;

    using std::bind;
    using DRing::exportable_callback;
    using DRing::CallSignal;
    using DRing::ConfigurationSignal;
    using DRing::PresenceSignal;
    using DRing::AudioSignal;

    using SharedCallback = std::shared_ptr<DRing::CallbackWrapperBase>;

    auto confM = configurationManager_.get();

#ifdef RING_VIDEO
    using DRing::VideoSignal;
#endif

    // Configuration event handlers
    auto registeredNameFoundCb = exportable_callback<ConfigurationSignal::RegisteredNameFound>([&]
        (const std::string& account_id, int state, const std::string& address, const std::string& name){
            auto remainingSessions = configurationManager_->getPendingNameResolutions(name);

            for(auto session: remainingSessions){
                const auto request = session->get_request();
                std::string body = address;
                const std::multimap<std::string, std::string> headers
                {
                    {"Content-Type", "text/html"},
                    {"Content-Length", std::to_string(body.length())}
                };
                if(address.size() > 0)
                    session->close(restbed::OK, body, headers);
                else
                    session->close(404);
            }
        });


    // This is a short example of a callback using a lambda. In this case, this displays the incoming messages
    const std::map<std::string, SharedCallback> configEvHandlers = {
        exportable_callback<ConfigurationSignal::IncomingAccountMessage>([]
            (const std::string& accountID, const std::string& from, const std::map<std::string, std::string>& payloads){
                RING_INFO("accountID : %s", accountID.c_str());
                RING_INFO("from : %s", from.c_str());
                RING_INFO("payloads");
                for(auto& it : payloads)
                    RING_INFO("%s : %s", it.first.c_str(), it.second.c_str());

            }),
            registeredNameFoundCb,
    };

    if (!DRing::init(static_cast<DRing::InitFlag>(flags)))
        return -1;

    registerSignalHandlers(configEvHandlers);

    // Dummy callbacks are registered for the other managers
    registerSignalHandlers(std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>>());
    registerSignalHandlers(std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>>());
#ifdef RING_VIDEO
    registerSignalHandlers(std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>>());
#endif

    if (!DRing::start())
        return -1;

    return 0;
}

void
RestClient::endLib() noexcept
{
    DRing::fini();
}

void
RestClient::initResources()
{
    // This is the function that initiates the resources.
    // Each resources is defined by a route and a void function with a shared pointer to the session as argument

    // In this case, here's an example of the default route. It will list all the managers available
    auto default_res = std::make_shared<restbed::Resource>();
    default_res->set_path("/");
    default_res->set_method_handler("GET", [](const std::shared_ptr<restbed::Session> session){

        RING_INFO("[%s] GET /", session->get_origin().c_str());

        std::string body = "Available routes are : \r\n/configurationManager\r\n/videoManager\r\n";

        const std::multimap<std::string, std::string> headers
        {
            {"Content-Type", "text/html"},
            {"Content-Length", std::to_string(body.length())}
        };
    });

    // And finally, we give the resource to the service to handle it
    service_.publish(default_res);

    // For the sake of convenience, each manager sends a vector of their resources
    for(auto& it : configurationManager_->getResources())
        service_.publish(it);

    for(auto& it : videoManager_->getResources())
        service_.publish(it);
}
