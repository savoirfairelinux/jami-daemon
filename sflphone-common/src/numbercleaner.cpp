/*
 *  Copyright (C) 2009 Savoir-Faire Linux inc.
 *
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

#include "numbercleaner.h"

#include <iostream>

NumberCleaner::NumberCleaner (void) : _prefix("") {
}

NumberCleaner::~NumberCleaner (void) {
}

std::string NumberCleaner::clean (std::string to_clean) {

    strip_char (" ", &to_clean);
    strip_char ("-", &to_clean);
    strip_char ("(", &to_clean);
    strip_char (")", &to_clean);

    return to_clean.insert (0, this->get_phone_number_prefix ());
}

void NumberCleaner::strip_char (std::string to_strip, std::string *num) {

    std::size_t pos;
    
    while ( (pos=(*num).find (to_strip)) != std::string::npos) {
        *num = (*num).erase (pos, 1);
    }
}
