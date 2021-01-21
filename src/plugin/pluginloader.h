/*
 *  Copyright (C) 2004-2021 Savoir-faire Linux Inc.
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
#pragma once

#include "jamiplugin.h"
#include <dlfcn.h>
#include <string>
#include <memory>

namespace jami {

class Plugin
{
public:
    virtual ~Plugin() = default;

    static Plugin* load(const std::string& path, std::string& error);
    virtual void* getSymbol(const char* name) const = 0;
    virtual JAMI_PluginInitFunc getInitFunction() const
    {
        return reinterpret_cast<JAMI_PluginInitFunc>(getSymbol(JAMI_DYN_INIT_FUNC_NAME));
    }

protected:
    Plugin() = default;
};

class DLPlugin : public Plugin
{
public:
    DLPlugin(void* handle, const std::string& path)
        : handle_(handle, ::dlclose)
        , path_ {path}
    {
        api_.context = this;
    }

    virtual ~DLPlugin() { unload(); }
    bool unload()
    {
        if (!handle_) {
            return false;
        }
        return ::dlclose(handle_.release());
    }

    void* getSymbol(const char* name) const
    {
        if (!handle_)
            return nullptr;

        return ::dlsym(handle_.get(), name);
    }

    const std::string& getPath() const { return path_; }

public:
    void* apiContext_;
    JAMI_PluginAPI api_;

private:
    std::unique_ptr<void, int (*)(void*)> handle_;
    const std::string path_;
};

} // namespace jami
