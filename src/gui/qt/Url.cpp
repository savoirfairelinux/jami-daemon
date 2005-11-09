/**
 *  Copyright (C) 2004-2005 Savoir-Faire Linux inc.
 *  Author: Jean-Philippe Barrette-LaPierre
 *             <jean-philippe.barrette-lapierre@savoirfairelinux.com>
 *                                                                              
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
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

#include <qurl.h>

#include "Url.hpp"

static uchar hex_to_int( uchar c )
{
    if ( c >= 'A' && c <= 'F' )
        return c - 'A' + 10;
    if ( c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if ( c >= '0' && c <= '9')
        return c - '0';
    return 0;
}

void Url::decode( QString& url )
{
    int oldlen = url.length();
    if ( !oldlen )
        return;

    int newlen = 0;

    std::string newUrl;

    int i = 0;
    while ( i < oldlen ) {
        ushort c = url[ i++ ].unicode();
        if ( c == '%' ) {
            c = hex_to_int( url[ i ].unicode() ) * 16 + hex_to_int( url[ i + 1 ].unicode() );
            i += 2;
        }
	else if ( c == '+' ) {
	  c = ' ';
	}
        newUrl += c;
	newlen++;
    }

    url = QString::fromUtf8(newUrl.c_str());
}
