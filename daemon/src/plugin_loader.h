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
#ifndef PLUGIN_LOADER_H
#define PLUGIN_LOADER_H

#include "ring_plugin.h"

#include <string>

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

#endif /* PLUGIN_LOADER_H */
