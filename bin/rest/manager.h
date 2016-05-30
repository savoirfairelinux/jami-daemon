#pragma once

#include <iostream>
#include <memory>
#include <unordered_set>
#include <boost/asio.hpp>

#include "connection.h"

/*
 *	The manager is here to handle all the connection, and delete them properly
 */
template<class T>
class Manager
{
	public:
		Manager() {}

		void start(std::shared_ptr<Connection<T> > c)
		{
			connections_.insert(c);
			c->start();
		}

		void stop(std::shared_ptr<Connection<T> > c)
		{
			connections_.erase(c);
			c->stop();
		}

		void stopAll()
		{
			for (auto c: connections_)
				c->stop();
			connections_.clear();
		}


	private:
		std::unordered_set<std::shared_ptr<Connection<T> >> connections_;
};
