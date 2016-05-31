#pragma once

#include <memory>
#include <chrono>
#include <map>
#include <vector>
#include <string>
#include <iostream>
#include <thread>
#include <functional>
#include <restbed>

#include "dring/dring.h"
#include "dring/callmanager_interface.h"
#include "dring/configurationmanager_interface.h"
#include "dring/presencemanager_interface.h"
#ifdef RING_VIDEO
#include "dring/videomanager_interface.h"
#endif
#include "logger.h"
#include "restconfigurationmanager.h"

class RestClient {
	public:
		RestClient(unsigned short port, int flags, bool persistent);
		~RestClient();

		int event_loop() noexcept;
		int exit() noexcept;

	private:
		int initLib(int flags);
		void endLib() noexcept;
		void initResources();

		bool pollNoMore_ = false;

		std::unique_ptr<RestConfigurationManager> configurationManager_;

		// Restbed
		restbed::Service service_;
		std::shared_ptr<restbed::Settings> settings_;

		// Restbed ressources
		std::shared_ptr<restbed::Resource> accountList_;
		void get_accountList(const std::shared_ptr<restbed::Session> session);
};
