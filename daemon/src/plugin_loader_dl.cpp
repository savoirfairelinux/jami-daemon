/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#include "plugin_loader.h"

#include <dlfcn.h>

Plugin::Plugin(void* handle) : handle_(handle)
{}

Plugin::~Plugin()
{
    if (handle_)
        ::dlclose(handle_);
}

void* Plugin::getSymbol(const std::string& name)
{
    if (!handle_)
        return NULL;

    return ::dlsym(handle_, name.c_str());
}

Plugin* Plugin::load(const std::string& path, std::string& error)
{
    if (path.empty()) {
        error = "Empty path";
        return NULL;
    }

    void *handle = ::dlopen(path.c_str(), RTLD_NOW);
    if (!handle) {
        error += "Failed to load \"" + path + '"';

        std::string dlError = ::dlerror();
        if(dlError.size())
            error += " (" + dlError + ")";
        return NULL;
    }

    return new Plugin(handle);
}
