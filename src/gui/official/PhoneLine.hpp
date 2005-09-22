#include <QObject>
#include <QMutex>
#include <string>

#include "Call.h"

class PhoneLine : public QObject
{
  Q_OBJECT
  
public:
  PhoneLine();
  ~PhoneLine();

  void call(const std::string &to);

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

signals:
  void selected();
  void unselected();

private:
  Call *mCall;
  QMutex mPhoneLineMutex;
}
