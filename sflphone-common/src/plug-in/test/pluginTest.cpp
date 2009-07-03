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

#include "../plugin.h"

#define MAJOR_VERSION   1
#define MINOR_VERSION   0

class PluginTest : public Plugin
{

    public:
        PluginTest (const std::string &name)
                :Plugin (name) {
        }

        virtual int initFunc (PluginInfo **info) {

            (*info)->_plugin = this;
            (*info)->_major_version = MAJOR_VERSION;
            (*info)->_minor_version = MINOR_VERSION;
            (*info)->_name = getPluginName();

            return 0;
        }
};

extern "C" Plugin* createPlugin (void)
{
    return new PluginTest ("mytest");
}

extern "C" void destroyPlugin (Plugin *p)
{
    delete p;
}
