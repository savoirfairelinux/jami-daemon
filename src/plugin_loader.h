/*
 *  Copyright (C) 2004-2016 Savoir-faire Linux Inc.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
 */
#ifndef PLUGIN_LOADER_H
#define PLUGIN_LOADER_H

#include "ring_plugin.h"

#include <string>

namespace ring {

class Plugin
{
    public:
        virtual ~Plugin() = default;

        static Plugin* load(const std::string& path, std::string& error);

        virtual void* getSymbol(const char* name) const = 0;
        virtual RING_PluginInitFunc getInitFunction() const {
            return reinterpret_cast<RING_PluginInitFunc>(getSymbol(RING_DYN_INIT_FUNC_NAME));
        };

    protected:
        Plugin() = default;
};

} // namespace ring

#endif /* PLUGIN_LOADER_H */
