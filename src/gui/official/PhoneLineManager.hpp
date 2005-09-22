#include <QObject>
#include <QMutex>
#include <utility>
#include <vector>

/**
 * This is the class that ma
 */

class PhoneLineManager : public QObject
{
  Q_OBJECT

public:
  /**
   * This function will make a call on the 
   * current line. If there's no selected
   * line, it will choose the first available.
   */
  void call(const QString &to);
  
  /**
   * This function will switch the lines. If the line
   * is invalid, it just do nothing.
   */
  void selectLine(unsigned int line);

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

  /**
   * This function hangup the 
   */

private:
  /**
   * Returns the PhoneLine in position line.
   */
  PhoneLine * getPhoneLine(unsigned int line);

private:
  Session mSession;
  Account mAccount;

  std::vector< PhoneLine * > mPhoneLines;
  QMutex mPhoneLinesMutex;

  PhoneLine *mCurrentLine;
  QMutex mCurrentLineMutex;
};


