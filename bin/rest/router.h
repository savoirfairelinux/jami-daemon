#pragma once

#include <string>
#include <functional>
#include <unordered_map>

class Router
{
	public:
		Router() {};

		void addRoute(std::string path, std::function<std::string()> func);

		static std::unordered_map<std::string, std::function<std::string()>> routes;
	private:
};
