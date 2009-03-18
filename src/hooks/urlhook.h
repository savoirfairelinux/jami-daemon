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

#ifndef URL_HOOK_H
#define URL_HOOK_H

#include <string>

class UrlHook {

    public:
        /**
         * Constructor
         */
        UrlHook ();

        /**
         * Destructor
         */
        ~UrlHook ();

    private:
        /* The command to execute when receiving the URL */
        std::string _command;
    
        /* The field to look for in the header */
        std::string _field;
};

#endif // URL_HOOK_H
