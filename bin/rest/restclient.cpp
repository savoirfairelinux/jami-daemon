#include "restclient.h"

RestClient::RestClient(unsigned short port, int flags, bool persistent) :
	service_()
{
	configurationManager_.reset(new RestConfigurationManager());

	if (initLib(flags) < 0)
        throw std::runtime_error {"cannot initialize libring"};

	initResources();

	settings_ = std::make_shared<restbed::Settings>();
    settings_->set_port(port);
    //settings_->set_worker_limit( 4 );
	settings_->set_default_header( "Connection", "close" );
}

RestClient::~RestClient()
{
	RING_INFO("destroying RestClient");
	exit();
}

int
RestClient::event_loop() noexcept
{
	RING_INFO("Rest client starting web service");
	service_.start(settings_);

	RING_INFO("Rest client starting to poll events");
	while(!pollNoMore_)
	{
		DRing::pollEvents();
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
	}
	return 0;
}

int
RestClient::exit() noexcept
{
    try {
		pollNoMore_ = true;
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
	if (!DRing::init(static_cast<DRing::InitFlag>(flags)))
        return -1;

	registerCallHandlers(std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>>());
    registerConfHandlers(std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>>());
    registerPresHandlers(std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>>());
#ifdef RING_VIDEO
    registerVideoHandlers(std::map<std::string, std::shared_ptr<DRing::CallbackWrapperBase>>());
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
	// Init the routes

	accountList_ = std::make_shared<restbed::Resource>();
    accountList_->set_path("/accountList");
    accountList_->set_method_handler("GET", std::bind(&RestClient::get_accountList, this, std::placeholders::_1));

	// Add the routes to the service
	service_.publish(accountList_);

}

// Restbed resources
void
RestClient::get_accountList(const std::shared_ptr<restbed::Session> session)
{
	RING_INFO("[%s] requesting accountList", session->get_origin().c_str());

	std::vector<std::string> accountList = configurationManager_->getAccountList();

	std::string body = "";

	for(auto& it : accountList)
	{
		body += "ringID : ";
		body += it;
		body += '\n';
	}

    session->close(restbed::OK, body, {{"Content-Length",std::to_string(body.length())}});
}
