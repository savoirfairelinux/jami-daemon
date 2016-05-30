#pragma once

#include <iostream>
#include <string>

#include "json.h"

using json = nlohmann::json;

class Response
{
	public:
		static std::string generate(json& req);

	private:
		static std::string daytime_();
};
