#pragma once

#include <iostream>
#include <string>
#include <thread>
#include <future>
#include <memory>
#include <cstdint>

#include <boost/asio.hpp>

#ifdef HTTPS
	#include <boost/asio/ssl.hpp>
#endif

#include "manager.h"
#include "connection.h"
#include "router.h"

namespace asio = boost::asio;

typedef asio::ip::tcp::socket HTTP;
#ifdef HTTPS
	typedef asio::ssl::stream<boost::asio::ip::tcp::socket> HTTPS;
#endif

template<class T>
class Server
{
	public:

		Server(unsigned short port) :
			port_(port), router_(), ios_(), manager_(),
			acceptor_(ios_, asio::ip::tcp::endpoint(asio::ip::tcp::v4(), port_))
		{
			// Default route, just in case
			addRoute("/", [](){
				return "";
			});
			accept_();
		}

		~Server()
		{
			stop_();
		}

		void run()
		{
			// Run the main process
			ios_.run();
		}

		void addRoute(std::string path, std::function<std::string()> func)
		{
			router_.addRoute(path, func);
		}

	private:
		void accept_()
		{
			auto c = std::make_shared<Connection<T> >(ios_, manager_);
			acceptor_.async_accept(c->socket(), [this, c](boost::system::error_code ec)
			{
				// Check whether the server was stopped by a signal before this
				// completion handler had a chance to run.
				if (!acceptor_.is_open())
				{
					RING_ERR("Acceptor not open");
					return;
				}

				if (!ec)
				{
					manager_.start(c);
				}
				else
				{
					RING_ERR("Error launching async_accept");
				}

				accept_();
			});

		}

		void stop_()
		{
			ios_.stop();
		}

		const unsigned short port_;
		unsigned int index_ = 0;
		asio::io_service ios_;
		asio::ip::tcp::acceptor acceptor_;
		Manager<T> manager_;
		Router router_;
};
