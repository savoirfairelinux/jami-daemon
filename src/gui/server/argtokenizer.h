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
#include <iostream>

typedef std::list<std::string> TokenList;

/**
Separate a string into token
a b "c d" = 3 tokens: [a], [b], [c d]

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

  TokenList tokenize(const std::string& str)
  {
     TokenList stack; // token stack
     std::string::size_type length = str.size();
     std::string::size_type i, pos;
     std::string temp;

     bool inToken = false; // if we are inside a token or not
     bool inQuote = false; // look if we are inside a "quoted-string"
     char lastChar = '\0'; // lastChar for \" escaping inside quote
     char c;

     pos = 0; // position inside quoted string, to escape backslashed-doublequote
     for (i=0;i<length;i++) {
       c = str[i];
       // for the new token
       if (inToken == false) {
         if (c == ' ') { continue; }   // escape space outside a token
         else if (c == '"') {
            inToken = true;
            inQuote = true;
            lastChar = '\0';
            pos = 0;
            continue;
         } else {
           inToken = true;
         }
       }
       if (inToken) {
       // we are inside a token
         if (inQuote) { // we are looking for a " token
           if ( c == '"' ) { 
             if (lastChar == '\\') { 
               temp[pos-1] = '"';
             } else { // end of the string
               if (temp.size()) { stack.push_back(temp); temp=""; }
               temp = "";
               inToken = false;
               inQuote = false;
             }
           } else { // normal character to append
             temp += c;
             pos++;
           }
           lastChar = c;
         } else { // not in quote, stop to first space
           if ( c == ' ' ) {
              if (temp.size()) { stack.push_back(temp); temp=""; }
              inToken = false;
           } else {
             temp += c;
           }
         }
       }
     }
     if (temp.size()) { stack.push_back(temp); } // add last keyword

     return stack;
  }

  // look at http://yansanmo.no-ip.org/test/cpp/argtokenizer.h
  // for test
};

#endif // __ARG_TOKENIZER__
