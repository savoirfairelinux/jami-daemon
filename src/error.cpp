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

#include <string>

#include "error.h"
#include "global.h"
#include "manager.h"
  
using namespace std;

Error::Error (Manager *mngr){
	_mngr = mngr;
	issetError = 0;
} 

int
Error::errorName (Error_enum num_name) {
	string str;
	switch (num_name){
		// Handle opening device errors
		case OPEN_FAILED_DEVICE:
			_mngr->displayErrorText("Open device failed ");
			issetError = 2; 
			break;
			
		// Handle setup errors
		case HOST_PART_FIELD_EMPTY:
			_mngr->displayError("Fill host part field");
			issetError = 2;
			break;	
		case USER_PART_FIELD_EMPTY:
			_mngr->displayError("Fill user part field");
			issetError = 2;
			break;
		case PASSWD_FIELD_EMPTY:
			_mngr->displayError("Fill password field");
			issetError = 2;
			break; 

		// Handle sip uri 
		case FROM_ERROR:
			_mngr->displayError("Error for 'From' header");
			issetError = 1;
			break;
		case TO_ERROR:
			_mngr->displayError("Error for 'To' header");
			issetError = 1;
			break;

		default:
			issetError = 0;
			break;
	}  
	return issetError;   
} 
  
