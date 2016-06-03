#include "restclient.h"

RestClient::RestClient(int port, int flags, bool persistent) :
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
	RING_INFO("RestClient running on port [%d]", port);
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
    // Get the resources from the configuration manager
	std::vector<std::shared_ptr<restbed::Resource>> configManagerResources = configurationManager_->getResources();

    // Add them the the service
	for(auto& it : configManagerResources)
		service_.publish(it);
}
