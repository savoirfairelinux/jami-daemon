/*
 *  Copyright (C) 2004-2006 Savoir-Faire Linux inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
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
#include <map>

#include "Account.hpp"

class Session
{
 public:
  Session();
  Session(const QString &id);
  ~Session();
  
  /**
   * retreive the account identified by name.
   */
  Account getAccount(const QString &name) const;

  Account getDefaultAccount() const;

  /**
   * Return the first or selected account object
   * or 0 if not found
   */
  Account* getSelectedAccount();

  /**
   * Set Selected Account ID
   * @param accountID account id
   */
  void setSelectedAccountID(const QString &accountID) {
    mSelectedAccountId = accountID;
  }

  /**
   * retreive account 
   */
  Request* getAccountList() const;

  /**
   * This function will play a tone. This is
   * just a ear candy.
   */
  Request *playDtmf(char c) const;


  /**
   * This function will retreive the given list.
   */
  Request *list(const QString &category) const;

  /**
   * This function will register to receive events
   */
  Request *getEvents() const;

  /**
   * This function will ask for all calls status.
   */
  Request *getCallStatus() const;

  /**
   * This function will mute the microphone.
   */
  Request *mute() const;

  /**
   * This function will ask sflphoned to close
   * the session. This will only close the session,
   * so sflphoned will still be running after.
   */
  Request *close() const;

  /**
   * This function will register with the default account.
   */
  Request *registerToServer(const QString&) const;

  /**
   * This function try to switch audio (sound) driver
   */
  Request *switchAudioDriver() const;

  /**
   * This function will stop sflphoned.
   */
  Request *stop() const;

  Request *configSet(const QString &section, const QString &name, const QString &value) const;
  Request *configSave() const;
  Request *configGetAll() const;

  /**
   * This function will set the volume to 
   * the given percentage
   */
  Request *volume(unsigned int volume) const;

  /**
   * This function will set the mic volume to 
   * the given percentage
   */
  Request *micVolume(unsigned int volume) const;

  /**
   * This function will unmute the microphone.
   */
  Request *unmute() const;

  /**
   * This function will ask to the SessionIO
   * linked to this session to connect.
   */
  void connect() const;

  QString id() const;
  Request *stopTone() const;
  Request *playTone() const;

 private:
  QString mId;
  QString mSelectedAccountId;
  std::map<QString, Account*> mAccountMap;
};

#endif
