#ifndef __PHONELINEMANAGERIMPL_HPP__
#define __PHONELINEMANAGERIMPL_HPP__

#include <Qt>
#include <QObject>
#include <QMutex>
#include <utility>
#include <vector>

class PhoneLine;

#include "Account.hpp"
#include "Session.hpp"

/**
 * This is the class that manages phone lines
 */
class PhoneLineManagerImpl : public QObject
{
  Q_OBJECT

public:
  PhoneLineManagerImpl();

  /**
   * This function will make a call on the 
   * current line. If there's no selected
   * line, it will choose the first available.
   */
  void call(const QString &to);
  

  /**
   * This function hangup the selected line.
   * If there's no current line, it does nothing.
   */
  void hangup();

  /**
   * This function hangup the line given in argument.
   * If the line is not valid, it doesn't do nothing.
   */
  void hangup(unsigned int line);

  PhoneLine *getPhoneLine(unsigned int line);

  void setNbLines(unsigned int line);

signals:
  void unselected(unsigned int);
  void selected(unsigned int);

public slots:
  void sendKey(Qt::Key c);

  /**
   * This function will switch the lines. If the line
   * is invalid, it just do nothing.
   */
  void selectLine(unsigned int line);
  
  void selectAvailableLine();

private:
  Session mSession;
  Account mAccount;

  std::vector< PhoneLine * > mPhoneLines;
  QMutex mPhoneLinesMutex;

  PhoneLine *mCurrentLine;
  QMutex mCurrentLineMutex;
};


#endif
