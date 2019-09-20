#pragma once

#include "def.h"

#include <string>

namespace DRing {
DRING_PUBLIC void loadPlugin(const std::string& path);
DRING_PUBLIC void unloadPlugin(const std::string& path);
}

