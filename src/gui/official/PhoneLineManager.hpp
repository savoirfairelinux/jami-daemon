#include <QObject>
#include <QMutex>
#include <utility>
#include <vector>

class PhoneLineManager : public QObject
{
  Q_OBJECT
  
public:
  void call(const QString &to);
  
  /**
   * This function will switch the lines.
   */
  void selectLine(int line);
  PhoneLine *getLine(int line);

private:
  Session mSession;
  Account mAccount;

  std::vector< PhoneLine * > mPhoneLines;
  QMutex mPhoneLinesMutex;

  PhoneLine *mCurrentLine;
  QMutex mCurrentLineMutex;
};
