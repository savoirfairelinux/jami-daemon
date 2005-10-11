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

#ifndef SFLPHONEGUI_SESSION_H
#define SFLPHONEGUI_SESSION_H

#include <qstring.h>

#include "Account.hpp"

class Session
{
 public:
  Session();
  Session(const QString &id);
  
  /**
   * retreive the account identified by name.
   */
  Account getAccount(const QString &name) const;

  Account getDefaultAccount() const;

  /**
   * This function will play a tone. This is
   * just a ear candy.
   */
  QString playDtmf(char c) const;

  /**
   * This function will register to receive events
   */
  QString getEvents() const;

  /**
   * This function will ask for all calls status.
   */
  QString getCallStatus() const;

  /**
   * This function will mute the microphone.
   */
  QString mute() const;

  /**
   * This function will set the volume to 
   * the given percentage
   */
  QString volume(unsigned int volume) const;

  /**
   * This function will set the mic volume to 
   * the given percentage
   */
  QString micVolume(unsigned int volume) const;

  /**
   * This function will unmute the microphone.
   */
  QString unmute() const;

  /**
   * This function will ask to the SessionIO
   * linked to this session to connect.
   */
  void connect() const;

  QString id() const;
  QString playTone() const;

 private:
  QString mId;
};

#endif
