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
#include "responsemessage.h"

/**
 * Used by isFinal() to check for the first digit
 * 2456 Means that 2XX, 4XX, 5XX and 6XX are final messages
 */
const std::string ResponseMessage::FINALCODE = "2456";

ResponseMessage::ResponseMessage(const std::string& code,
      const std::string& seq,
      const std::string& message) : _code(code), _seq(seq), _message(message)
{
}

/*
 * Construct a message with a list of argument
 * and a space separator between each argument
 */
ResponseMessage::ResponseMessage(const std::string& code,
      const std::string& seq,
      TokenList& arg) : _code(code), _seq(seq)
{
  TokenList::iterator iter=arg.begin();
  if (iter!=arg.end()) {
    _message = *iter;
    iter++;
  }
  // add space between each
  while(iter!=arg.end()) {
    _message.append(" ");
    // TODO: encode string here
    _message.append(*iter);
    iter++;
  }
}


ResponseMessage::~ResponseMessage()
{
}

/**
 * Prepare and return a message, ready to be send to the client
 * @return message that contains: code sequenceId message
 */
std::string 
ResponseMessage::toString() const
{
  return _code + " " + _seq + " " + _message;
}

/**
 * Return true when the response message is final (no more coming message for a request)
 * Note: the code should be 3 numbers-long.
 * @return true if it's the last message of the request
 */
bool 
ResponseMessage::isFinal() const
{
  bool final = false;
  // only 3 chars code are valid
  // code that begin by: 2,4,5,6 are final
  
  if (_code.size() == 3) {
    if ( FINALCODE.find(_code[0]) != std::string::npos ) {
      final = true;
    }
  }
  return final;
}


