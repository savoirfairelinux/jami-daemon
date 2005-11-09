/**
 *  Copyright (C) 2004-2005 Savoir-Faire Linux inc.
 *  Author: Jean-Philippe Barrette-LaPierre
 *             <jean-philippe.barrette-lapierre@savoirfairelinux.com>
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
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __TCPSESSIONIO_HPP__
#define __TCPSESSIONIO_HPP__

#include <qobject.h>
#include <qstring.h>
#include <qsocket.h>
#include <qtextstream.h>
#include <qtimer.h>
#include <list>

#include "SessionIO.hpp"

#ifdef QT3_SUPPORT
#include <Q3Socket>
typedef Q3Socket QSocket;
#else
#include <qsocket.h>
#endif


class TCPSessionIO : public SessionIO
{
  Q_OBJECT

public:
  TCPSessionIO(const QString &hostname, 
	       Q_UINT16 port);

  virtual ~TCPSessionIO();

signals:
  void connected();
  void disconnected();
  
public slots:
  /**
   * This function send the request that we were
   * unable to send.
   */
  void sendWaitingRequests();

  /**
   * Those function are the actual function
   * that write to the socket.
   */
  virtual void send(const QString &request);

  /**
   * This function is called when we have 
   * incomming data on the socket.
   */
  virtual void receive();

  /**
   * Those function are the actual function
   * that read from the socket.
   */
  virtual void receive(QString &answer);
  virtual void connect();

  void resetConnectionTries();

 private slots:
  /**
   * This function is called when we have an error
   * on the socket.
   */
 void error(int);

private:
  QSocket *mSocket;
  QString mHostname;
  Q_UINT16 mPort;

  std::list< QString > mStack;

  unsigned int mNbConnectTries;
  QTimer *mReconnectTimer;
};

#endif
