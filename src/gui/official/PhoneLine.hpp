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

  std::string getCallId();

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

  std::string getLineStatus();
  
public slots:
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

  void setState(const std::string &){}
  void setPeer(const std::string &){}


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

  std::string mLineStatus;
};
