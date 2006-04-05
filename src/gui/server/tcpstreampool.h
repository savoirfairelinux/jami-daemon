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
#ifndef TCPSTREAMPOOL_H
#define TCPSTREAMPOOL_H

#include <string>
#include <cc++/socket.h>

#include "ObjectPool.hpp"

/**
 * Utilisation:
 * TCPSessionIO session = TCPStreamPool(aServer);
 * std::string response = "hello";
 * std::string request;
 * session.start();
 * session.send(response);
 * while(session.receive(request)) {
 *   std::cout << request << std::endl;
 * }
 * @author Yan Morin
*/
class TCPStreamPool : public ost::TCPSession 
{
public:
  TCPStreamPool(ost::TCPSocket& server) : ost::TCPSession(server) 
  {
    setCancel(cancelDeferred);
  }
  ~TCPStreamPool();

  void run();
  void send(const std::string& response);
  void sendLast();
  bool receive(std::string& request);

private:
  ObjectPool<std::string> _outputPool;
  ObjectPool<std::string> _inputPool;
};

#endif
