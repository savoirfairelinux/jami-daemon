#include "parserHTTP.h"

#include <iostream>

json
ParserHTTP::parse(std::string& request)
{
	json ret;

	// Slice the request in lines
	std::vector<std::string> tokens = tokenize_(request, '\n');
	// Cleaning the end of the request
	tokens.pop_back();
	tokens.pop_back();

	// First line is always [METHODE] /path [HTTP VERSION]
	tokens[0].pop_back();
	std::vector<std::string> head = tokenize_(tokens[0], ' ');

	if(!isMethod_(head[0]))
		throw std::runtime_error("Method is not acceptable");

	ret["Method"] = head[0];
	ret["Path"] = head[1];
	ret["HTTP"] = head[2].substr(5);

	// Consume the token
	tokens.erase(tokens.begin());

	for(auto &s: tokens)
	{
		// Cleaning the /r
		s.pop_back();
		if(isHost_(s))
		{
			std::vector<std::string> addr = tokenize_(s, ' ');
			ret["Host"] = addr[1];
		}
		else if(containsAccept_(s))
		{
			std::vector<std::string> accept = tokenize_(s, ':');
			std::vector<std::string> list = tokenize_(accept[1], ',');

			for(auto& it : list)
			{
				if(it.front() == ' ')
					it.erase(0, 1);

			}
			ret[accept[0]] = list;
		}
		else
		{
			std::vector<std::string> line = tokenize_(s, ':');
			auto key = *line.begin();
			line.erase(line.begin());

			if(key != "User-Agent")
			{
				std::vector<std::string> list = tokenize_(line[0], ',');
				for(auto& it : list)
				{
					if(it.front() == ' ')
						it.erase(0, 1);

				}
				(list.size() == 1 ? ret[key] = list[0] : ret[key] = list);
			}
			else
			{
				ret[key] = line[0];
			}

		}
	}

	return ret;
}

std::vector<std::string>
ParserHTTP::tokenize_(const std::string& str, const char delim)
{
    std::vector<std::string> lines;
    std::stringstream ss(str);
    std::string line;
    while(std::getline(ss, line, delim)) {
        lines.push_back(line);
    }
    return lines;
}

inline bool
ParserHTTP::isMethod_(const std::string& m)
{
	return m == "POST" ||
			m == "GET" ||
			m == "PUT" ||
			m == "PATCH" ||
			m == "DELETE";
}

inline bool
ParserHTTP::isHost_(const std::string& h)
{
	return h.substr(0, 4) == "Host";
}

inline bool
ParserHTTP::containsAccept_(const std::string& a)
{
	return a.substr(0, 6) == "Accept";
}
