#include "globals.h"

#include "PhoneLine.hpp"
#include "PhoneLineButton.hpp"
#include "Requester.hpp"
#include "SessionIOFactory.hpp"
#include "SFLLcd.hpp"
#include "SFLPhoneApp.hpp"
#include "SFLPhoneWindow.hpp"
#include "SFLRequest.hpp"
#include "TCPSessionIOCreator.hpp"
#include "VolumeControl.hpp"



SFLPhoneApp::SFLPhoneApp(int argc, char **argv)
  : QApplication(argc, argv)
{
  SessionIOFactory::instance().setCreator(new TCPSessionIOCreator(QString("localhost"), 3999));
  PhoneLineManager::instance().initialize();
  PhoneLineManager::instance().setNbLines(NB_PHONELINES);
  Requester::instance().registerObject< Request >(QString("playtone"));
  Requester::instance().registerObject< Request >(QString("stoptone"));
  Requester::instance().registerObject< Request >(QString("playdtmf"));
  Requester::instance().registerObject< CallRequest >(QString("call"));
  Requester::instance().registerObject< EventRequest >(QString("getevents"));
  Requester::instance().registerObject< CallStatusRequest >(QString("getcallstatus"));
  Requester::instance().registerObject< PermanentRequest >(QString("answer"));
  Requester::instance().registerObject< PermanentRequest >(QString("notavailable"));
  Requester::instance().registerObject< PermanentRequest >(QString("refuse"));
  Requester::instance().registerObject< PermanentRequest >(QString("hangup"));
  Requester::instance().registerObject< TemporaryRequest >(QString("unmute"));
  Requester::instance().registerObject< TemporaryRequest >(QString("hold"));
  Requester::instance().registerObject< TemporaryRequest >(QString("unhold"));
  Requester::instance().registerObject< TemporaryRequest >(QString("senddtmf"));
  Requester::instance().registerObject< Request >(QString("setspkrvolume"));
  Requester::instance().registerObject< Request >(QString("setmicvolume"));
  Requester::instance().registerObject< Request >(QString("mute"));
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
  QObject::connect(w->mMute, SIGNAL(clicked(bool)),
		   &PhoneLineManager::instance(), SLOT(mute(bool)));
  QObject::connect(w->mHangup, SIGNAL(clicked()),
		   &PhoneLineManager::instance(), SLOT(hangup()));
  QObject::connect(w->mHold, SIGNAL(clicked()),
		   &PhoneLineManager::instance(), SLOT(hold()));
  QObject::connect(w->mClear, SIGNAL(clicked()),
		   &PhoneLineManager::instance(), SLOT(clear()));
  QObject::connect(w, SIGNAL(keyPressed(Qt::Key)),
		   &PhoneLineManager::instance(), SLOT(sendKey(Qt::Key)));

  // LCD Connections.
  QObject::connect(&PhoneLineManager::instance(), SIGNAL(lineStatusSet(QString)),
		   w->mLcd, SLOT(setLineStatus(QString)));
  QObject::connect(&PhoneLineManager::instance(), SIGNAL(unselectedLineStatusSet(QString)),
		   w->mLcd, SLOT(setUnselectedLineStatus(QString)));
  QObject::connect(&PhoneLineManager::instance(), SIGNAL(actionSet(QString)),
		   w->mLcd, SLOT(setAction(QString)));
  QObject::connect(&PhoneLineManager::instance(), SIGNAL(globalStatusSet(QString)),
		   w->mLcd, SLOT(setGlobalStatus(QString)));
  QObject::connect(&PhoneLineManager::instance(), SIGNAL(bufferStatusSet(QString)),
		   w->mLcd, SLOT(setBufferStatus(QString)));


  //Volume connections
  QObject::connect(w->mVolume, SIGNAL(valueUpdated(int)),
		   &PhoneLineManager::instance(), SLOT(setVolume(int)));
  QObject::connect(w->mMicVolume, SIGNAL(valueUpdated(int)),
		   &PhoneLineManager::instance(), SLOT(setMicVolume(int)));
  QObject::connect(&PhoneLineManager::instance(), SIGNAL(volumeUpdated(int)),
		   w->mVolume, SLOT(setValue(int)));
  QObject::connect(&PhoneLineManager::instance(), SIGNAL(micVolumeUpdated(int)),
		   w->mMicVolume, SLOT(setValue(int)));



  QObject::connect(&PhoneLineManager::instance(), SIGNAL(disconnected()),
		   w, SLOT(askReconnect()));
  QObject::connect(w, SIGNAL(reconnectAsked()),
		   &PhoneLineManager::instance(), SLOT(connect()));

  QObject::connect(&PhoneLineManager::instance(), SIGNAL(gotErrorOnCallStatus(QString)),
		   w, SLOT(askResendStatus(QString)));
  QObject::connect(&PhoneLineManager::instance(), SIGNAL(gotErrorOnGetEvents(QString)),
		   w, SLOT(askResendStatus(QString)));
  QObject::connect(w, SIGNAL(resendStatusAsked()),
		   &PhoneLineManager::instance(), SIGNAL(readyToSendStatus()));


}

