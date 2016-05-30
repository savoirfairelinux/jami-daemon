#pragma once

#include <string>
#include <vector>
#include <sstream>

#include "json.h"

using json = nlohmann::json;

class ParserHTTP
{
	public:
		ParserHTTP() {};

		static json parse(std::string& request);

	private:
		static std::vector<std::string> tokenize_(const std::string& str, const char delim);

		static bool isMethod_(const std::string& m);
		static bool isHost_(const std::string& h);
		static bool containsAccept_(const std::string& a);

};
