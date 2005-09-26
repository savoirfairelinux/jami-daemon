#include "globals.h"

#include "SFLPhoneApp.hpp"
#include "SFLPhoneWindow.hpp"
#include "PhoneLine.hpp"
#include "PhoneLineButton.hpp"
#include "Requester.hpp"



SFLPhoneApp::SFLPhoneApp(int argc, char **argv)
  : QApplication(argc, argv)
  , mPhoneLineManager(NB_PHONELINES)
  , mSession()
  , mAccount(mSession.getDefaultAccount())
{
  Requester::instance().registerObject< Request >(std::string("sendtone"));
  Requester::instance().registerObject< CallRelatedRequest >(std::string("call"));
}

void
SFLPhoneApp::initConnections(SFLPhoneWindow *w)
{
  // We connect the phone line buttons to the PhoneLineManager
  unsigned int i = 0;
  for(std::list< PhoneLineButton * >::iterator pos = w->mPhoneLineButtons.begin();
      pos != w->mPhoneLineButtons.end();
      pos++) {
    PhoneLine *line = mPhoneLineManager.getPhoneLine(i);
    QObject::connect(*pos, SIGNAL(clicked(unsigned int)),
		     &mPhoneLineManager, SLOT(selectLine(unsigned int)));
    QObject::connect(line, SIGNAL(selected()),
		     *pos, SLOT(press()));
    QObject::connect(line, SIGNAL(unselected()),
		     *pos, SLOT(release()));
    QObject::connect(line, SIGNAL(backgrounded()),
		     *pos, SLOT(suspend()));

    i++;
  }

  QObject::connect(w, SIGNAL(keyPressed(Qt::Key)),
		   &mPhoneLineManager, SLOT(sendKey(Qt::Key)));
}
