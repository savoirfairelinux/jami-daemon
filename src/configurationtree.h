//
// (c) 2004 Jerome Oufella <jerome.oufella@savoirfairelinux.com>
// (c) 2004 Savoir-faire Linux inc.
//
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
	ConfigurationTree (const char *);
	~ConfigurationTree (void);	
	ConfigSection*  head (void) { return this->_head; }
	int				populateFromFile(const char*);
	int				saveToFile		(const char*);
	int				setValue		(const char*, const char*, int);
	int				setValue		(const char*, const char*, const char*);
	char*			getValue		(const char*, const char*);
	
private:
	ConfigSection *_head;
};

#endif  // __CONFIGURATION_TREE_H__

// EOF
