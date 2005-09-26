//
// C++ Interface: tcpsessionio
//
// Description: 
//
//
// Author: Yan Morin <yan.morin@savoirfairelinux.com>, (C) 2005
//
// Copyright: See COPYING file that comes with this distribution
//
//
#ifndef TCPSESSIONIO_H
#define TCPSESSIONIO_H

#include <cc++/socket.h>
#include "sessionio.h"
#include "tcpstreampool.h"

/**
@author Yan Morin
*/
class TCPSessionIO : public SessionIO
{
public:
    TCPSessionIO();
    ~TCPSessionIO();

    void send(const std::string& response);
    bool receive(std::string& request);
    bool good();
    void init();

private:
  ost::TCPSocket* _serverSocket;
  TCPStreamPool* _clientStream;

  static const int PORT;
  static const char * const IP;
};

#endif
