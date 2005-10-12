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
#ifndef __RESPONSEMESSAGE_H__
#define __RESPONSEMESSAGE_H__

#include <string>
#include "argtokenizer.h" // for TokenList declare

/**
Response Message stock a message from a request when it is executed.
@author Yan Morin
*/
class ResponseMessage
{
public:
    // default constructor with empty seq/code/message
    ResponseMessage() {}
    // build a constructor with a TokenList
    // so that they will be encoded..
    ResponseMessage(const std::string& code,const std::string& seq, const std::string& message);
    ResponseMessage(const std::string& code,const std::string& seq, TokenList& arg);
    ~ResponseMessage();

    std::string sequenceId() const { return _seq; }

    std::string toString() const;
    bool isFinal() const;
private:
    // append an encoded string to the message
    void appendMessage(const std::string& strToken);

    // 3 numbers long code
    std::string _code;
    std::string _seq;
    std::string _message;

    static const std::string FINALCODE;
};

#endif
