#include <QChar>
#include <QObject>
#include <QMutex>
#include <string>

#include "Session.hpp"

class Call;

class PhoneLine : public QObject
{
  Q_OBJECT
  
public:
  PhoneLine(const Session &session, unsigned int line);
  ~PhoneLine();

  void call(const std::string &to);
  void call();
  void hangup();
  void hold();

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

  void sendKey(Qt::Key c);
  
public slots:
  /**
   * The user selected this line.
   */
  void select();

  /**
   * This phoneline is no longer selected.
   */
  void unselect();


signals:
  void selected();
  void unselected();
  void backgrounded();

private:
  Session mSession;
  Call *mCall;
  QMutex mPhoneLineMutex;
  unsigned int mLine;

  bool mSelected;
  bool mInUse;
  //This is the buffer when the line is not in use;
  std::string mBuffer;
};
