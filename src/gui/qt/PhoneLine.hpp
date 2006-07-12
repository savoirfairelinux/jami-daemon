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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <qobject.h>
#include <qstring.h>
#include <qtimer.h>
#include <qdatetime.h>

#include "Account.hpp"
#include "Session.hpp"

class Call;

class PhoneLine : public QObject
{
  Q_OBJECT
  
public:
  PhoneLine(const Session &session, 
	    unsigned int line);
  ~PhoneLine();

  void call(const QString &to);
  void call();
  void answer();
  void hangup(bool sendrequest = true);
  void hold();
  void unhold();

  QString getCallId();

  unsigned int line();

  /**
   * This will lock the current phone line.
   * 
   * Note: this will only lock the phone line
   * for those that uses this lock, unlock
   * mechanism. PhoneLineManager always uses
   * this mechanism. So, if you work only with
   * PhoneLineManager, it will be thread safe.
   */
  void lock();

  /**
   * This will unlock the current phone line.
   * See the Note of the lock function.
   */
  void unlock();


  /**
   * This function will return true if there's no 
   * activity on this line. It means that even 
   * if we typed something on this line, but haven't
   * started any communication, this will be available.
   */
  bool isAvailable()
  {return !mCall;}

  bool isTalking()
  {return mTalking;}

  void sendKey(Qt::Key c);

  QTime getTalkingTime()
  {return mTalkingTime;}

  QString getLineStatus();
  QString getBuffer()
  {return mBuffer;}
  
public slots:
  void setLineStatus(QString);
  void setAction(QString);
  void setTempAction(QString);
  void resetAction();
  void incomming(const Call &call);

  /**
   * Clears the buffer of the line.
   */
  void clear();
  
  /**
   * The user selected this line.
   */
  void select(bool hardselect = false);

  /**
   * This phoneline is no longer selected.
   */
  void unselect(bool hardselect = false);

  /**
   * This will do a hard unselect. it means it
   * will remove the call if there's one.
   */
  void disconnect();

  /**
   * This will close the current call. it means it
   * will remove the call if there's one.
   */
  void close();

  /**
   * This will close the current call. it means it
   * will remove the call if there's one. The line
   * will be in an error state.
   */
  void error(QString);

  /**
   * This function will put the line on hold
   * and will wait for the numbre to compose.
   */
  void transfer();

  void finishTransfer();

  void setPeer(const QString &peer);
  void clearPeer();
  void setState(const QString &){}

  void setTalkingState();
  void stopTalking();

signals:
  void selected(bool);
  void backgrounded();
  void lineStatusChanged(QString);
  void actionChanged(QString);
  void bufferStatusChanged(QString);
  void peerUpdated(QString);
  void peerCleared();
  void talkingStarted(QTime);
  void talkingStopped();
  void transfered();
  /** when the call is hangup */
  void hanguped();

private:
  void setCall(Call *call);
  void setCall(const Call &call);
  void clearCall();

  Session mSession;
  Call *mCall;
  unsigned int mLine;

  bool mSelected;
  bool mInUse;
  //This is the buffer when the line is not in use;
  QString mBuffer;

  QString mLineStatus;
  QString mAction;
  QTimer *mActionTimer;
  QTime mTalkingTime;
  bool mTalking;
  QString mPeer;

  bool mIsOnError;
  bool mIsTransfering;
};
