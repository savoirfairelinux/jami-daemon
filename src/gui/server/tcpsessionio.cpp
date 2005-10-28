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
#include "tcpsessionio.h"
#include <cstdio>

const int TCPSessionIO::PORT = 3999;
const char * const TCPSessionIO::IP = "127.0.0.1";

TCPSessionIO::TCPSessionIO() : SessionIO()
{
  _clientStream = 0;
  ost::InetAddress addr(IP);

  //Creating a listening socket
  try {
    _serverSocket = new ost::TCPSocket(addr, PORT);
  } catch( ost::Socket *e ) {
    throw e;
  }
}


TCPSessionIO::~TCPSessionIO()
{
  fprintf(stderr, "TCPSessionIO: delete clientStream\n");
  delete _clientStream; _clientStream = NULL;
  fprintf(stderr, "TCPSessionIO: delete serverSocket\n");
  delete _serverSocket; _serverSocket = NULL;
  fprintf(stderr, "TCPSessionIO: end\n");
}

bool
TCPSessionIO::good()
{
  if (_clientStream) { // just in case
    return _clientStream->good();
  }
  return false;
}

void 
TCPSessionIO::send(const std::string& response)
{
  if (_clientStream) { // just in case
    _clientStream->send(response);
  }
}

bool 
TCPSessionIO::receive(std::string& request)
{
  bool returnValue = false;
  if (_clientStream) { // just in case
    returnValue = _clientStream->receive(request);
  }
  return returnValue;
}

void
TCPSessionIO::init() {
  // this is strange to create a waiting client here...
  _clientStream = new TCPStreamPool(*_serverSocket);
  _clientStream->start();
}
