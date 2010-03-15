/*
 * validator.h
 *
 *  Created on: 2010-03-12
 *      Author: jb
 */

#ifndef VALIDATOR_H_
#define VALIDATOR_H_

#include <string>
#include <iostream>

using namespace std;

class Validator {
  public:
	static bool isNumber(std::string str);
	static bool isNotNull(std::string str);
};
#endif /* VALIDATOR_H_ */
