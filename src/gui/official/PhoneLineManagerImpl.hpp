#ifndef __PHONELINEMANAGERIMPL_HPP__
#define __PHONELINEMANAGERIMPL_HPP__

//#include <qt.h>
#include <qobject.h>
#include <qmutex.h>
#include <utility>
#include <vector>

class PhoneLine;

#include "Account.hpp"
#include "Call.hpp"
#include "EventFactory.hpp"
#include "Session.hpp"

/**
 * This is the class that manages phone lines
 */
class PhoneLineManagerImpl : public QObject
{
  Q_OBJECT

public:
  PhoneLineManagerImpl();
  ~PhoneLineManagerImpl();

  /**
   * Will return the PhoneLine linked to the line 
   * number.
   */
  PhoneLine *getPhoneLine(unsigned int line);

  /**
   * Will return the PhoneLine with the call ID.
   * If there's no PhoneLine of call ID, it will
   * return NULL.
   */
  PhoneLine *getPhoneLine(const QString &callId);

  PhoneLine *getCurrentLine();

  void setNbLines(unsigned int line);

signals:
  void unselected(unsigned int);
  void selected(unsigned int);
  void connected();
  void disconnected();
  void readyToSendStatus();
  void readyToHandleEvents();
  void gotErrorOnCallStatus();
  void globalStatusSet(QString);
  void bufferStatusSet(QString);
  void actionSet(QString);
  void lineStatusSet(QString);

  void volumeUpdated(int);
  void micVolumeUpdated(int);

public slots:
  /**
   * You need to call this function once. It must be
   * call before doing anything in this class.
   * 
   * NOTE: You need to set the creator to SessionIOFactory
   * before calling this function.
   */
  void initialize();

  /**
   * This function will make the process to start.
   * It will connect to the server, and start the
   * event handling.
   */
  void start();

  /**
   * This will ask the session to connect
   * to the sflphone server.
   */
  void connect();

  void sendKey(Qt::Key c);

  /**
   * This function will put the current line
   * on hold. If there's no current line,
   * it will do nothing.
   */
  void hold();

  /**
   * This function will hanp up the current line
   * If there's no current line, it will do nothing.
   */
  void hangup();

  /**
   * This function will mute the microphone if muting
   * is true, it will unmute otherwise.
   */
  void mute(bool);

  /**
   * This function will mute the microphone
   */
  void mute();

  /**
   * This function will unmute the microphone
   */
  void unmute();

  /**
   * This function will hanp up the line number given 
   * argument. Be aware that the first line is 1, not 
   * zero.
   */
  void hangup(unsigned int line);

  /**
   * This function will hanp up the line with the
   * following call ID. If there's no line with 
   * the call ID, it will do nothing.
   */
  void hangup(const QString &callId);

  /**
   * This function will make a call on the 
   * current line. If there's no selected
   * line, it will choose the first available.
   */
  void call(const QString &to);

  /**
   * This function will add an incomming call
   * on a phone line.
   */
  void incomming(const QString &accountId,
		 const QString &callId,
		 const QString &peer);

  /**
   * This function is used to add a call on a 
   * phone line.
   */
  void addCall(Call call,
	       const QString &peer, 
	       const QString &state);
  void addCall(const QString &accountId, 
	       const QString &callId, 
	       const QString &peer, 
	       const QString &state);

  /**
   * This function will make a call on the 
   * current line. If there's no selected
   * line. It will do nothing. It will call 
   * the destination contained in the
   * PhoneLine buffer, if any. 
   */
  void call();

  /**
   * This function will switch the lines. If the line
   * is invalid, it just do nothing.
   */
  void selectLine(unsigned int line, 
		  bool hardselect = false);

  /**
   * This function will switch the line to the line having
   * the given call id. If the line is invalid, it just do 
   * nothing.
   */
  void selectLine(const QString &callId,
		  bool hardselect = false);

  /**
   * This function will clear the buffer of the active
   * line. If there's no active line, it will do nothing.
   */
  void clear();
  
  /**
   * This function will return the next available line.
   * The line is locked, So you'll need to unlock it.
   */
  PhoneLine *getNextAvailableLine();

  /**
   * This function will return the PhoneLine with the 
   * given id. If there's no such line, it will return 
   * NULL. The line is locked, So you'll need to unlock it.
   */
  PhoneLine *getLine(unsigned int line);

  /**
   * This function will return the PhoneLine with the
   * given call id. If there's no such line, it will 
   * return NULL. The line is locked, So you'll need to 
   * unlock it.
   */
  PhoneLine *getLine(const Call &call);

  /**
   * This function will return the next available line.
   * The line is NOT locked.
   */
  PhoneLine *selectNextAvailableLine();

  /**
   * This function will send the getevents request
   * to the server.
   *
   * NOTE: This function MUST be called AFTER getcallstatus's
   * completion.
   */
  void handleEvents();

  /**
   * This function will simply signal the volume change.
   */
  void updateVolume(int volume);

  /**
   * This function will send a request to sflphoned
   * to change the volume to the given percentage.
   */
  void setVolume(int volume);

  /**
   * This function will simply signal the mic volume change.
   */
  void updateMicVolume(int volume);

  /**
   * This function will send a request to sflphoned
   * to change the mic volume to the given percentage.
   */
  void setMicVolume(int volume);

  void errorOnCallStatus()
  {emit gotErrorOnCallStatus();}

 private slots:
  /**
   * This will send all the command needed when a
   * connection has just been established. 
   */
  void startSession();

  /**
   * This function is called when we are disconnected
   * from the server. This will unselect all phone lines. 
   */
  void closeSession();


private:
  void isInitialized();

private:
  Session *mSession;
  Account *mAccount;

  std::vector< PhoneLine * > mPhoneLines;
  QMutex mPhoneLinesMutex;

  PhoneLine *mCurrentLine;
  QMutex mCurrentLineMutex;

  bool mIsInitialized;
  QMutex mIsInitializedMutex;

  int mVolume;
  int mMicVolume;
};


#endif
