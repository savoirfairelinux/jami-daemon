#pragma once

#include <iostream>
#include <memory>
#include <string>

#include <boost/asio.hpp>

#include "manager.h"
#include "router.h"
#include "parserHTTP.h"
#include "json.h"
#include "response.h"

#include "logger.h"

namespace asio = boost::asio;

template<class T>
class Manager;

/*
 *	Each Connection represents a request from the client
 */
template<class T>
class Connection : public std::enable_shared_from_this<Connection<T> >
{


	public:
		Connection(boost::asio::io_service& io_service, Manager<T> manager):
			socket_(io_service), manager_(manager)
		{
		}

		T& socket()
		{
			return socket_;
		}

		void start()
		{
			read_();
		}

		void stop()
		{
		}

	private:
		void read_()
		{
			asio::streambuf::mutable_buffers_type bufs = buffer_.prepare(512);

			auto self(this->shared_from_this());
			socket_.async_read_some(bufs,
				[this, self](boost::system::error_code ec, std::size_t bytes)
				{
					if(!ec)
					{

						// Parsing the request
						buffer_.commit(bytes);
						asio::streambuf::const_buffers_type bufs = buffer_.data();
						std::string data(asio::buffers_begin(bufs), asio::buffers_end(bufs));

						RING_DBG("%s", data.c_str());

						buffer_.consume(buffer_.size());

						nlohmann::json parsed = ParserHTTP::parse(data);

						std::string msg = Response::generate(parsed);
						write_(msg);
					}
					else if(ec != asio::error::operation_aborted)
					{
						manager_.stop(this->shared_from_this());
					}
				});
		}

		void write_(std::string& msg)
		{
			auto self(this->shared_from_this());
			asio::async_write(socket_, asio::buffer(msg.c_str(), msg.size()),
				[this, self](boost::system::error_code ec, std::size_t bytes)
				{
					if(!ec)
					{
						boost::system::error_code ignored;
						socket_.shutdown(T::shutdown_both, ignored);
					}

					if (ec != asio::error::operation_aborted)
					{
						manager_.stop(this->shared_from_this());
					}
				});
		}

		T socket_;
		boost::asio::streambuf buffer_;
		Manager<T> manager_;
};
