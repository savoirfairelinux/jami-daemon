/*
 *  Copyright (C) 2004-2006 Savoir-Faire Linux inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author: Jean-Philippe Barrette-LaPierre
 *             <jean-philippe.barrette-lapierre@savoirfairelinux.com>
 *                                                                              
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *                                                                              
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *                                                                              
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __SFLPHONEAPP_HPP__
#define __SFLPHONEAPP_HPP__

#include <qapplication.h>
#include <qsettings.h>
#include <qevent.h>

#include "PhoneLineManager.hpp"
#include "Session.hpp"
#include "Account.hpp"

class SFLPhoneWindow;
class Launcher;
class NumericKeypad;

class SFLPhoneApp : public QApplication
{
  Q_OBJECT

public:
  SFLPhoneApp(int argc, char **argv);
  virtual ~SFLPhoneApp();

  /**
   * This function will make the widgets 
   * connections.
   */
  void initConnections(SFLPhoneWindow *w);
  void loadSkin();

  void launch();

public slots:
  /**
   * Handle argc/argv if any left
   */
  void handleArg();
  void paste();
  void shortcutPressed(QKeyEvent* e);

signals:
  void registerFailed(const QString);
  void registerSucceed(const QString);

private:

  Launcher *mLauncher;
  NumericKeypad *mKeypad;
};

#endif 
