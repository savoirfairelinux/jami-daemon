/*
 *  Copyright (C) 2004-2018 Savoir-faire Linux Inc.
 *
 *  Author: Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 * USA.
 */

#include "pluginloader.h"

#include <dlfcn.h>
#include <memory>

namespace jami {

class DLPlugin : public Plugin {
public:
  DLPlugin(void *handle) : handle_(handle, ::dlclose){}
  void *getSymbol(const char *name) const;

private:
  std::unique_ptr<void, int (*)(void *)> handle_;
};

void *DLPlugin::getSymbol(const char *name) const {
  if (!handle_)
    return nullptr;

  return ::dlsym(handle_.get(), name);
}

Plugin *Plugin::load(const std::string &path, std::string &error) {
  if (path.empty()) {
    error = "Empty path";
    return nullptr;
  }

  // Clear any existing error
  ::dlerror();

  void *handle = ::dlopen(path.c_str(), RTLD_NOW);
  if (!handle) {
    error += "Failed to load \"" + path + '"';

    std::string dlError = ::dlerror();
    if (dlError.size())
      error += " (" + dlError + ")";
    return nullptr;
  }

  return new DLPlugin(handle);
}

} // namespace jami
