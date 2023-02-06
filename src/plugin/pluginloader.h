/*
 *  Copyright (C) 2004-2023 Savoir-faire Linux Inc.
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

/**
 * @class Plugin
 * @brief This class is used to attempt loading a plugin library.
 */
class Plugin
{
public:
    virtual ~Plugin() = default;

    /**
     * @brief Load plugin's library.
     * @return DLPlugin if success.
     */
    static Plugin* load(const std::string& path, std::string& error);
    virtual void* getSymbol(const char* name) const = 0;

    /**
     * @brief Search loaded library for its initialization function
     * @return Plugin's initialization function.
     */
    virtual JAMI_PluginInitFunc getInitFunction() const
    {
        return reinterpret_cast<JAMI_PluginInitFunc>(getSymbol(JAMI_DYN_INIT_FUNC_NAME));
    }

protected:
    Plugin() = default;
};

/**
 * @class  DLPlugin
 * @brief This class is used after a plugin library is successfully loaded.
 */
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

    /**
     * @brief Unload plugin's library.
     * @return True if success.
     */
    bool unload()
    {
        if (!handle_) {
            return false;
        }
        return !(::dlclose(handle_.release()));
    }

    /**
     * @brief Searchs for symbol in library.
     * @param name
     * @return symbol.
     */
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
    // Pointer to the loaded library returned by dlopen
    std::unique_ptr<void, int (*)(void*)> handle_;
    // Plugin's data path
    const std::string path_;
};

} // namespace jami
