#include "globals.h"

#include "SFLPhoneApp.hpp"
#include "SFLPhoneWindow.hpp"
#include "PhoneLineButton.hpp"



SFLPhoneApp::SFLPhoneApp(int argc, char **argv)
  : QApplication(argc, argv)
  , mPhoneLineManager(NB_PHONELINES)
  , mSession()
  , mAccount(mSession.getDefaultAccount())
{}

void
SFLPhoneApp::initConnections(SFLPhoneWindow *w)
{
  unsigned int i = 0;
  for(std::list< PhoneLineButton * >::iterator pos = w->mPhoneLineButtons.begin();
      pos != w->mPhoneLineButtons.end();
      pos++) {
    QObject::connect(*pos, SIGNAL(clicked(unsigned int)),
		     &mPhoneLineManager, SLOT(selectLine(unsigned int)));
    i++;
  }
}
