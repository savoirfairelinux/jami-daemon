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

#include <QMutex>
#include <QString>
#include <QTcpSocket>
#include <QTextStream>
#include <list>

#include "SessionIO.hpp"


class TCPSessionIO : public SessionIO
{
  Q_OBJECT

public:
  TCPSessionIO(const QString &hostname, 
	       quint16 port);

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
  virtual void send(const std::string &request);

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
  virtual void receive(std::string &answer);
  virtual void connect();

 private slots:
  /**
   * This function is called when we have an error
   * on the socket.
   */
  void error();

private:
  QTcpSocket *mSocket;
  QString mHostname;
  quint16 mPort;

  QMutex mStackMutex;
  std::list< QString > mStack;
};

#endif
