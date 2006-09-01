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

#ifndef __SESSIONIO_HPP__
#define __SESSIONIO_HPP__

#include <qobject.h>
#include <qstring.h>

/**
 * This is the main class that will handle 
 * the IO.
 */
class SessionIO : public QObject
{
  Q_OBJECT
  
public:
  virtual ~SessionIO(){}

public slots:
  virtual void connect() {}

  /**
   * You can use this function for sending request.
   * The sending is non-blocking. This function will
   * send the data as it is; it will NOT add an EOL.
   * the stream will be "sync"ed.
   */
  virtual void send(const QString &request) = 0;

  /**
   * You can use this function to receive answers.
   * This function will wait until there's an 
   * answer to be processed.
   */
  virtual void receive(QString &answer) = 0;

signals:
  void firstConnectionFailed();
};



#endif

