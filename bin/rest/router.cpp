#include "router.h"

std::unordered_map<std::string, std::function<std::string()>> Router::routes;

void
Router::addRoute(std::string path, std::function<std::string()> func)
{
	// We don't add empty response because of a weird bug
	if(func().size() != 0)
		Router::routes.insert({path, func});
}
