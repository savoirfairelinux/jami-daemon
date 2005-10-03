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

#include "globals.h"
#include "Requester.hpp"
#include "TCPSessionIO.hpp"

TCPSessionIO::TCPSessionIO(const QString &hostname, quint16 port)
  : mSocket(new QTcpSocket(this))
  , mHostname(hostname)
  , mPort(port)
{
  QObject::connect(mSocket, SIGNAL(readyRead()),
		   this, SLOT(receive()));
  QObject::connect(mSocket, SIGNAL(connected()),
		   this, SLOT(sendWaitingRequests()));
  QObject::connect(mSocket, SIGNAL(connected()),
		   this, SIGNAL(connected()));
  QObject::connect(mSocket, SIGNAL(error(QAbstractSocket::SocketError)),
		   this, SLOT(error()));
  QObject::connect(mSocket, SIGNAL(error(QAbstractSocket::SocketError)),
		   this, SIGNAL(disconnected()));
}

TCPSessionIO::~TCPSessionIO()
{}

void 
TCPSessionIO::error()
{
  _debug("TCPSessionIO: %s. \n", 
	 mSocket->errorString().toStdString().c_str());
  mSocket->close();
}

void 
TCPSessionIO::receive()
{
  QString s;
  receive(s);
  Requester::instance().receiveAnswer(s);
}

void
TCPSessionIO::connect()
{
  _debug("TCPSessionIO: Tring to connect to %s:%d.\n", 
	 mHostname.toStdString().c_str(), 
	 mPort);
  mSocket->connectToHost(mHostname, mPort);
}


void
TCPSessionIO::sendWaitingRequests()
{
  _debug("TCPSessionIO: Connected.\n");
  QTextStream stream(mSocket);
  QMutexLocker guard(&mStackMutex);
  while(mSocket->state() == QAbstractSocket::ConnectedState &&
	mStack.size() > 0) {
    stream << *mStack.begin();
    mStack.pop_front();
  }
}

void
TCPSessionIO::send(const QString &request)
{
  QTextStream stream(mSocket);
  if(mSocket->state() == QAbstractSocket::ConnectedState) {
    _debug("TCPSessioIO: Sending request to sflphone: %s", 
	   request.toStdString().c_str());
    stream << request;
  }
  else {
    mStackMutex.lock();
    mStack.push_back(request);
    mStackMutex.unlock();
  }
}

void
TCPSessionIO::receive(QString &answer)
{
  if(mSocket->isReadable()) {
    QTextStream stream(mSocket);
    answer = stream.readLine();
    _debug("TCPSessionIO: Received answer from sflphone: %s\n", 
	   answer.toStdString().c_str());
  }
}







