/*
 *  Copyright (C) 2009 Savoir-Faire Linux inc.
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


#ifndef PLUGIN_H
#define PLUGIN_H

#include "global.h" 

#include "pluginmanager.h" 

/*
 * @file plugin.h
 * @brief Define a plugin object 
 */

class Plugin {

    public:
        Plugin( const std::string &name ){
            _name = name;
        }

        virtual ~Plugin()  {}

        inline std::string getPluginName (void) { return _name; }

        /**
         * Return the minimal core version required so that the plugin could work
         * @return int  The version required
         */
        virtual int initFunc (PluginInfo **info) = 0;

    private:
        Plugin &operator =(const Plugin &plugin);

        std::string _name;
};

typedef Plugin* createFunc (void);

typedef void destroyFunc (Plugin*);

#endif //PLUGIN_H

