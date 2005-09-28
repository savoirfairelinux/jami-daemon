/**
  *  Copyright (C) 2004-2005 Savoir-Faire Linux inc.
  *  Author: Jerome Oufella <jerome.oufella@savoirfairelinux.com>
  *                                                                               *  This program is free software; you can redistribute it and/or modify
  *  it under the terms of the GNU General Public License as published by
  *  the Free Software Foundation; either version 2 of the License, or
  *  (at your option) any later version.
  *                                                                               *  This program is distributed in the hope that it will be useful,
  *  but WITHOUT ANY WARRANTY; without even the implied warranty of
  *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  *  GNU General Public License for more details.
  *                                                                               *  You should have received a copy of the GNU General Public License
  *  along with this program; if not, write to the Free Software
  *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
  */


// The configuration tree holds all the runtime parameters for sflphone.
//
// Its _head pointer is a list of N Sections, N >= 0
// Each Section holds M ConfigItems, M >= 0
// 
// Each Section or Item's name is stored in its _key pointer.
//
// It looks like that :
//
// HEAD  _______________  _next 
//   ---| ConfigSection |------->...-->NULL
//      |_______________|	    
//         | _head              
//	 	  _|________            
//	 	 |ConfigItem|
//		 |__________|--_next->...--->NULL
//

#ifndef __CONFIGURATION_TREE_H__
#define __CONFIGURATION_TREE_H__

#include "configitem.h"

class ConfigurationTree {
public:
	ConfigurationTree (void);
	ConfigurationTree (const std::string&);
	~ConfigurationTree (void);	
	ConfigSection*  head (void) { return this->_head; }
	int				populateFromFile(const std::string& );
	int				saveToFile		(const std::string& );
	int				setValue		(const std::string& , const std::string& , int);
	int				setValue(const std::string& , const std::string& , const std::string& );
	std::string			getValue		(const std::string& , const std::string& );
	
private:
	ConfigSection *_head;
};

#endif  // __CONFIGURATION_TREE_H__

// EOF
