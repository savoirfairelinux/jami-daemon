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
#ifndef SUBCALL_H
#define SUBCALL_H

#include <string>

/**
Contains an sequenceId and a callid. It's allow us to maintain a map of id->SubCall

@author Yan Morin
*/
class SubCall{
public:
  SubCall() {}
  SubCall(const std::string& seq, const std::string& callid) : _seq(seq), _callid(callid) {}
  ~SubCall() {}

  // accessors
  std::string sequenceId() const { return _seq; }
  std::string callId() const { return _callid; }
private:
  std::string _seq; // sequence id
  std::string _callid;
};

#endif
