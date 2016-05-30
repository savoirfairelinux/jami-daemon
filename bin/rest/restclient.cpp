#ifdef HAVE_CONFIG_H
#include "config.h"
#endif // HAVE_CONFIG_H

#include "restclient.h"

#include <chrono>

#include "logger.h"

RestClient::RestClient(unsigned short port, int flags, bool persistent)
{
	if(initLibrary(flags) < 0)
		throw std::runtime_error {"cannot initialize libring"};

	std::thread pollEvents_([this](){
		while(!pollNoMore_)
		{
			DRing::pollEvents();
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
	});

#if HTTPS
	server_.reset(new Server<HTTPS> {port});
#else
	server_.reset(new Server<HTTP> {port});
#endif


	RING_INFO("HTTP server running on port %d", port);

	server_->addRoute("/", [](){
			return "Hi, this is the defaut route from the Muffin HTTP server example";
	});
}

RestClient::~RestClient()
{
	pollNoMore_ = true;
	pollEvents_.join();

	exit();
}

int
RestClient::event_loop() noexcept
{
	return 0;
}

int
RestClient::exit() noexcept
{
    try {
        finiLibrary();
    } catch (const std::exception& err) {
        std::cerr << "quitting: " << err.what() << std::endl;
        return 1;
    }

    return 0;
}

int
RestClient::initLibrary(int flags)
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
RestClient::finiLibrary() noexcept
{
	DRing::fini();
}
