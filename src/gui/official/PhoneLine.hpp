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

signals:
  void selected();
  void unselected();

private:
  Call *mCall;
  QMutex mCallMutex;

 
}
