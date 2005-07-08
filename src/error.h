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

#ifndef __ERROR_H__
#define __ERROR_H__

#include <stdio.h>


typedef enum {
	OPEN_FAILED_DEVICE = 0,
	
	HOST_PART_FIELD_EMPTY,
	USER_PART_FIELD_EMPTY,
	PASSWD_FIELD_EMPTY,

	FROM_ERROR,
	TO_ERROR

} Error_enum;

class Manager;
class Error {
public: 
	Error (Manager *mngr); 
	~Error (void) {};

	int errorName (Error_enum);
	inline int 	getError (void) 	{ return issetError; }
	inline void setError(int err) 	{ issetError = err; }

private:
	Manager *_mngr;
	int 	issetError;
	
};

#endif // __ERROR_H__
