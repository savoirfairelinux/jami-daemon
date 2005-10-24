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
#include "tcpstreampool.h"
#include "../../global.h"

#define WAITING_TIME 10UL

TCPStreamPool::~TCPStreamPool() 
{
  terminate();
  std::string output;
  while (good() && _outputPool.pop(output, WAITING_TIME))  {
    *this << output << std::endl;
  }
}
void 
TCPStreamPool::run() {
  std::string output;
  std::string input;
  std::string null = ""; // we don't want empty string 
  char cr13 = '\r'; // we don't want carriage return in empty line

  while(!testCancel() && good()) {
    while (isPending(ost::TCPSocket::pendingInput, WAITING_TIME)) {
      std::getline(*this, input);
      //_debug("TCPStreamPool getline %s\n", input.c_str());
      if (input != null && input[0]!=cr13) {
        _inputPool.push(input);
      }
      // security check, since we are inside a loop
      if (testCancel() || !good()) {break;}
    }
    if (_outputPool.pop(output, WAITING_TIME)) {
      //_debug("TCPStreamPool send %s\n", output.c_str());
      *this << output << std::endl;
    }
  }
}

void 
TCPStreamPool::send(const std::string& response)
{
 if (!testCancel()) {
   _outputPool.push(response);
 }
}

bool 
TCPStreamPool::receive(std::string& request)
{
  return _inputPool.pop(request, WAITING_TIME);
}


