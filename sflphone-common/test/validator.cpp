/*
 * validator.cpp
 *
 *  Created on: 2010-03-12
 *      Author: jb
 */

#include "validator.h"

bool Validator::isNumber(std::string str) {
	unsigned int i = 0;
	if (!str.empty() && (str[i] == '-' || str[i] == '+'))
		i++;
	return string::npos == str.find_first_not_of(".eE0123456789", i);
}

bool Validator::isNotNull(std::string str) {
	if(!str.empty())
		return true;
	else
		return false;
}
