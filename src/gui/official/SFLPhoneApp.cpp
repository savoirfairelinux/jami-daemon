#include "globals.h"

#include "SFLPhoneApp.hpp"
#include "SFLPhoneWindow.hpp"
#include "SFLRequest.hpp"
#include "PhoneLine.hpp"
#include "PhoneLineButton.hpp"
#include "Requester.hpp"



SFLPhoneApp::SFLPhoneApp(int argc, char **argv)
  : QApplication(argc, argv)
  , mSession()
  , mAccount(mSession.getDefaultAccount())
{
  Requester::instance().registerObject< Request >(std::string("playtone"));
  Requester::instance().registerObject< Request >(std::string("playdtmf"));
  Requester::instance().registerObject< EventRequest >(std::string("getevents"));
  Requester::instance().registerObject< CallRelatedRequest >(std::string("senddtmf"));
  Requester::instance().registerObject< CallRelatedRequest >(std::string("playdtmf"));
  Requester::instance().registerObject< CallRelatedRequest >(std::string("call"));
  Requester::instance().registerObject< CallRelatedRequest >(std::string("hold"));
  Requester::instance().registerObject< CallRelatedRequest >(std::string("unhold"));
  Requester::instance().registerObject< CallRelatedRequest >(std::string("hangup"));
  PhoneLineManager::instance().setNbLines(NB_PHONELINES);
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
}
