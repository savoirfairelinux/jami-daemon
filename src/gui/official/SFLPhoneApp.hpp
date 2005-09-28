#ifndef __SFLPHONEAPP_HPP__
#define __SFLPHONEAPP_HPP__

#include <QApplication>

#include "PhoneLineManager.hpp"
#include "Session.hpp"
#include "Account.hpp"

class SFLPhoneWindow;

class SFLPhoneApp : public QApplication
{
public:
  SFLPhoneApp(int argc, char **argv);

  /**
   * This function will make the widgets 
   * connections.
   */
  void initConnections(SFLPhoneWindow *w);

};

#endif 
