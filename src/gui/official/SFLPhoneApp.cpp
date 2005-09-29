#include "globals.h"

#include "PhoneLine.hpp"
#include "PhoneLineButton.hpp"
#include "Requester.hpp"
#include "SessionIOFactory.hpp"
#include "SFLPhoneApp.hpp"
#include "SFLPhoneWindow.hpp"
#include "SFLRequest.hpp"
#include "TCPSessionIOCreator.hpp"



SFLPhoneApp::SFLPhoneApp(int argc, char **argv)
  : QApplication(argc, argv)
{
  SessionIOFactory::instance().setCreator(new TCPSessionIOCreator(QString("localhost"), 3999));
  PhoneLineManager::instance().initialize();
  PhoneLineManager::instance().setNbLines(NB_PHONELINES);
  Requester::instance().registerObject< Request >(std::string("playtone"));
  Requester::instance().registerObject< Request >(std::string("playdtmf"));
  Requester::instance().registerObject< EventRequest >(std::string("getevents"));
  Requester::instance().registerObject< CallStatusRequest >(std::string("getcallstatus"));
  Requester::instance().registerObject< CallRelatedRequest >(std::string("answer"));
  Requester::instance().registerObject< CallRelatedRequest >(std::string("notavailable"));
  Requester::instance().registerObject< CallRelatedRequest >(std::string("refuse"));
  Requester::instance().registerObject< CallRelatedRequest >(std::string("senddtmf"));
  Requester::instance().registerObject< CallRelatedRequest >(std::string("playdtmf"));
  Requester::instance().registerObject< CallRelatedRequest >(std::string("call"));
  Requester::instance().registerObject< CallRelatedRequest >(std::string("hold"));
  Requester::instance().registerObject< CallRelatedRequest >(std::string("unhold"));
  Requester::instance().registerObject< CallRelatedRequest >(std::string("hangup"));
  PhoneLineManager::instance().start();
}

void
SFLPhoneApp::initConnections(SFLPhoneWindow *w)
{
  // We connect the phone line buttons to the PhoneLineManager
  unsigned int i = 0;
  for(std::list< PhoneLineButton * >::iterator pos = w->mPhoneLineButtons.begin();
      pos != w->mPhoneLineButtons.end();
      pos++) {
    PhoneLine *line = PhoneLineManager::instance().getPhoneLine(i);
    QObject::connect(*pos, SIGNAL(clicked(unsigned int)),
		     &PhoneLineManager::instance(), SLOT(selectLine(unsigned int)));
    QObject::connect(line, SIGNAL(selected()),
		     *pos, SLOT(press()));
    QObject::connect(line, SIGNAL(unselected()),
		     *pos, SLOT(release()));
    QObject::connect(line, SIGNAL(backgrounded()),
		     *pos, SLOT(suspend()));

    i++;
  }

  QObject::connect(w->mOk, SIGNAL(clicked()),
		   &PhoneLineManager::instance(), SLOT(call()));
  QObject::connect(w->mHangup, SIGNAL(clicked()),
		   &PhoneLineManager::instance(), SLOT(hangup()));
  QObject::connect(w->mHold, SIGNAL(clicked()),
		   &PhoneLineManager::instance(), SLOT(hold()));
  QObject::connect(w->mClear, SIGNAL(clicked()),
		   &PhoneLineManager::instance(), SLOT(clear()));
  QObject::connect(w, SIGNAL(keyPressed(Qt::Key)),
		   &PhoneLineManager::instance(), SLOT(sendKey(Qt::Key)));
  QObject::connect(&PhoneLineManager::instance(), SIGNAL(disconnected()),
		   w, SLOT(askReconnect()));
  QObject::connect(w, SIGNAL(reconnectAsked()),
		   &PhoneLineManager::instance(), SLOT(connect()));

  QObject::connect(&PhoneLineManager::instance(), SIGNAL(gotErrorOnCallStatus()),
		   w, SLOT(askResendStatus()));
  QObject::connect(w, SIGNAL(resendStatusAsked()),
		   &PhoneLineManager::instance(), SIGNAL(readyToSendStatus()));

}

