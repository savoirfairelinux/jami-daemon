%header %{

#include "dring/dring.h"
#include "dring/plugin_manager_interface.h"
%}

namespace DRing {
void loadPlugin(const std::string& path);
void unloadPlugin(const std::string& path);
void togglePlugin(const std::string& path, bool toggle);
std::vector<std::map<std::string,std::string>> getPluginPreferences(const std::string& path);
}
