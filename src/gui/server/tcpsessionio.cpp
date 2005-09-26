//
// C++ Implementation: tcpsessionio
//
// Description: 
//
//
// Author: Yan Morin <yan.morin@savoirfairelinux.com>, (C) 2005
//
// Copyright: See COPYING file that comes with this distribution
//
//
#include "tcpsessionio.h"
#include "../../global.h"


const int TCPSessionIO::PORT = 3999;
const char * const TCPSessionIO::IP = "127.0.0.1";

TCPSessionIO::TCPSessionIO() : SessionIO()
{
  _clientStream = 0;
  ost::InetAddress addr(IP);

  //Creating a listening socket
  _serverSocket = new ost::TCPSocket(addr, PORT);
}


TCPSessionIO::~TCPSessionIO()
{
  delete _clientStream;
  delete _serverSocket;
}

bool
TCPSessionIO::good()
{
  if (_clientStream) { // just in case
    _debug("_clientStream->good() == %d\n", _clientStream->good());
    return _clientStream->good();
  }
  _debug("_clientStream doesn't exists yet...");
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
  if (_clientStream) { // just in case
    return _clientStream->receive(request);
  }
  return false;
}

void
TCPSessionIO::init() {
  // this is strange to create a waiting client here...
  _clientStream = new TCPStreamPool(*_serverSocket);
  _clientStream->start();
}
