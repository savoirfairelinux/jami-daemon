#pragma once

#include <memory>
#include <thread>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif // HAVE_CONFIG_H
#include "dring.h"
#include "callmanager_interface.h"
#include "configurationmanager_interface.h"
#include "presencemanager_interface.h"
#ifdef RING_VIDEO
#include "videomanager_interface.h"
#endif

#include "server.h"

class RestClient {
	public:
		RestClient(unsigned short port, int flags, bool persistent);
		~RestClient();

		int event_loop() noexcept;
		int exit() noexcept;

	private:
		int initLibrary(int flags);
		void finiLibrary() noexcept;

#if HTTPS
		std::unique_ptr<Server<HTTPS>> server_;
#else
		std::unique_ptr<Server<HTTP>> server_;
#endif
		std::thread pollEvents_;
		bool pollNoMore_ = false;
};
