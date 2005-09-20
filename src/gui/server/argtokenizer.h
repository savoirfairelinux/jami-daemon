/**
 *  Copyright (C) 2005 Savoir-Faire Linux inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
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

#ifndef __ARG_TOKENIZER__
#define __ARG_TOKENIZER__

#include <list>
#include <string>

typedef std::list<std::string> TokenList;

/**
Separate a string into token
a b c%20d = 3 tokens: [a], [b], [c d]

Example:
#include <argtokenizer.h>

ArgTokenizer tokenizer;
TokenList tList = tokenizer.tokenize(std::string("bla bla bla"));
TokenList::iterator iter = tList.begin();
while( iter != tList.end() ) {
  std::cout << *iter << std::endl;
}
*/
class ArgTokenizer {
public:
  ArgTokenizer() {}   //  ctor
  ~ArgTokenizer() {}  //  dtor

  /**
   * Tokenize a string into a list of string
   * Separators are: space, newline and tab ( ,\n,\t)
   * @author: Jean-Philippe Barrette-LaPierre
   */
  TokenList tokenize(const std::string& str);
};

#endif // __ARG_TOKENIZER__
