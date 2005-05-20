/**
 *  Copyright (C) 2004-2005 Savoir-Faire Linux inc.
 *  Author: Laurielle Lea <laurielle.lea@savoirfairelinux.com>
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


#include "global.h"
#include "skin.h"

#ifndef PROGSHAREDIR
#error "You must run configure to define PREFIX and PROGSHAREDIR."
#endif

const char* PIXMAP_LINE_NAMES[] = {
	PIXMAP_LINE0_OFF,
	PIXMAP_LINE0_BUSY,
	PIXMAP_LINE1_OFF,
	PIXMAP_LINE1_BUSY,
	PIXMAP_LINE2_OFF,
	PIXMAP_LINE2_BUSY,
	PIXMAP_LINE3_OFF,
	PIXMAP_LINE3_BUSY,
	PIXMAP_LINE4_OFF,
	PIXMAP_LINE4_BUSY,
	PIXMAP_LINE5_OFF,
	PIXMAP_LINE5_BUSY,
};

Skin::Skin (void) {
}

Skin::~Skin (void) {
}

string 
Skin::getPath (const string & prefix, const string & progname, 
	const string & skindir, const string & skin, 
	const string & filename) {
	return (prefix + "/" + progname + "/" + skindir + "/" + skin + "/" + 
		filename);
}

string 
Skin::getPath (const string & skindir, const string & skintype, 
	const string & filename) {
	return (string(PROGSHAREDIR) + "/" + skindir + "/" + skintype + "/" + filename);
}
		
string 
Skin::getPath (const string & dir) {
	return (string(PROGSHAREDIR) + "/" + dir);
}

string 
Skin::getPathPixmap (const string & pixdir, const string & filename) {
	return (string(PROGSHAREDIR) + "/" + pixdir + "/" + filename);
}

string 
Skin::getPathRing (const string & ringdir, const string & filename) {
	return (string(PROGSHAREDIR) + "/" + ringdir + "/" + filename);
}

// EOF
