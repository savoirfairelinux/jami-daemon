#ifndef __PHONELINEMANAGERIMPL_HPP__
#define __PHONELINEMANAGERIMPL_HPP__

#include <Qt>
#include <QObject>
#include <QMutex>
#include <utility>
#include <vector>

class PhoneLine;

#include "Account.hpp"
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
  PhoneLine *getPhoneLine(const std::string &callId);

  PhoneLine *getCurrentLine();

  void setNbLines(unsigned int line);

signals:
  void unselected(unsigned int);
  void selected(unsigned int);

public slots:
  void initialize();
  void start();
  
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
  void hangup(const std::string &callId);

  /**
   * This function will make a call on the 
   * current line. If there's no selected
   * line, it will choose the first available.
   */
  void call(const QString &to);

  void incomming(const std::string &accountId,
		 const std::string &origin,
		 const std::string &callId);


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
  void selectLine(unsigned int line);

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
   * This function will return the next available line.
   * The line is NOT locked.
   */
  PhoneLine *selectNextAvailableLine();

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
};


#endif
