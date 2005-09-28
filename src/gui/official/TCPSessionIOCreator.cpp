#include "TCPSessionIOCreator.hpp"
#include "PhoneLineManager.hpp"

TCPSessionIOCreator::TCPSessionIOCreator(const QString &hostname, 
					 quint16 port)
  : mHostname(hostname)
  , mPort(port)
{}
  
TCPSessionIO *
TCPSessionIOCreator::create()
{
  TCPSessionIO *t = new TCPSessionIO(mHostname, mPort);
  QObject::connect(t, SIGNAL(connected()),
		   &PhoneLineManager::instance(), SLOT(start()));
  t->connect();
  return t;
}
