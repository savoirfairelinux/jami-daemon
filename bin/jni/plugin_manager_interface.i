%header %{

#include "dring/dring.h"
#include "dring/plugin_manager_interface.h"
%}

namespace DRing {
void loadPlugin(const std::string& path);
void unloadPlugin(const std::string& path);
void togglePlugin(const std::string& path, bool toggle);
}
