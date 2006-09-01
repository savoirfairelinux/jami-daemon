/*
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
#include "DebugOutput.hpp"
#include "Requester.hpp"
#include "TCPSessionIO.hpp"

#define NB_MAX_TRIES 4

TCPSessionIO::TCPSessionIO(const QString &hostname, Q_UINT16 port)
  : mSocket(new QSocket(this))
  , mHostname(hostname)
  , mPort(port)
  , mNbConnectTries(0)
  , mNbConnectTriesTotal(0)
{
  mReconnectTimer = new QTimer(this);
  QObject::connect(mReconnectTimer, SIGNAL(timeout()), 
		   this, SLOT(connect()));

  QObject::connect(mSocket, SIGNAL(readyRead()),
		   this, SLOT(receive()));
  QObject::connect(mSocket, SIGNAL(connected()),
		   this, SLOT(sendWaitingRequests()));
  QObject::connect(mSocket, SIGNAL(connected()),
		   this, SLOT(resetConnectionTries()));
  QObject::connect(mSocket, SIGNAL(connected()),
		   this, SIGNAL(connected()));
  QObject::connect(mSocket, SIGNAL(connectionClosed()),
		   this, SIGNAL(disconnected()));
  QObject::connect(mSocket, SIGNAL(error(int)),
		   this, SLOT(error(int)));
}

TCPSessionIO::~TCPSessionIO()
{}

void 
TCPSessionIO::resetConnectionTries()
{
  mNbConnectTries = 0;
}

void 
TCPSessionIO::error(int err)
{
  if (mNbConnectTriesTotal == 0) {
    emit firstConnectionFailed();
    mReconnectTimer->start(1000, true);
    mNbConnectTriesTotal++;
  } else {
    mNbConnectTriesTotal++;
    mNbConnectTries++;
    if(mNbConnectTries >= NB_MAX_TRIES) {
      DebugOutput::instance() << QObject::tr("TCPSessionIO: Connection failed: %1\n").arg(err);
      mNbConnectTries = 0;
      emit disconnected();
    } else {
     mReconnectTimer->start(2000, true);
    }
  }
  //mSocket->close();
}

void 
TCPSessionIO::receive()
{
  QString s;
  while(mSocket->canReadLine()) {
    receive(s);
    Requester::instance().receiveAnswer(s);
  }
}

void
TCPSessionIO::connect()
{
  DebugOutput::instance() << QObject::tr("TCPSessionIO: Tring to connect to %1:%2.\n")
    .arg(mHostname)
    .arg(mPort);
  mSocket->connectToHost(mHostname, mPort);
}

void
TCPSessionIO::sendWaitingRequests()
{
  DebugOutput::instance() << QObject::tr("TCPSessionIO: Connected.\n");
  QTextStream stream(mSocket);
  while(mSocket->state() == QSocket::Connected &&
	mStack.size() > 0) {
    stream << *mStack.begin();
    mStack.pop_front();
    mSocket->flush();
  }
}

void
TCPSessionIO::send(const QString &request)
{
  QTextStream stream(mSocket);
  if(mSocket->state() == QSocket::Connected) {
//    DebugOutput::instance() << QObject::tr("TCPSessioIO: Sending request to sflphone: %1")
//      .arg(request);
    stream << request;
    mSocket->flush();
  }
  else {
    mStack.push_back(request);
  }
}

void
TCPSessionIO::receive(QString &answer)
{
  if(mSocket->isReadable()) {
    QTextStream stream(mSocket);
    answer = stream.readLine();
//    DebugOutput::instance() << QObject::tr("TCPSessionIO: Received answer from sflphone: %1\n")
//      .arg(answer);
  }
}







