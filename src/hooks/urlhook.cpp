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

#include "urlhook.h"
#include <iostream>

UrlHook::UrlHook () { }

UrlHook::~UrlHook () { }

bool UrlHook::addAction (pjsip_msg *msg, std::string field, std::string command){

    std::string command_bg, value, url;
    pjsip_generic_string_hdr * hdr;
    size_t pos;

    std::cout << "SIP field: " << field << " - command: " << command << std::endl;; 
    
    /* Get the URL in the SIP header */
    if ( (hdr = (pjsip_generic_string_hdr*)this->url_hook_fetch_header_value (msg, field)) != NULL)
    {
        value = hdr->hvalue.ptr;
        if ( (pos=value.find ("\n")) != std::string::npos) {
            url = value.substr (0, pos);
        
            /* Execute the command in the background to not block the application */
            command_bg = command + " " + url + "&" ;
            /* Execute a system call */
            RUN_COMMAND (command_bg.c_str());

            return true;
        }
        else
            return false;
    }

    return false;
}

void* UrlHook::url_hook_fetch_header_value (pjsip_msg *msg, std::string field) {

    pj_str_t name;

    std::cout << "url hook fetch header value" << std::endl;

    /* Convert the field name into pjsip type */
    name = pj_str ((char*)field.c_str());

    /* Get the header value and convert into string*/
    return pjsip_msg_find_hdr_by_name (msg, &name, NULL);
}
