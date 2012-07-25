/******************************************************************************
 *   Copyright (C) 2012 by Savoir-Faire Linux                                 *
 *   Author : Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com>   *
 *                                                                            *
 *   This library is free software; you can redistribute it and/or            *
 *   modify it under the terms of the GNU Lesser General Public               *
 *   License as published by the Free Software Foundation; either             *
 *   version 2.1 of the License, or (at your option) any later version.       *
 *                                                                            *
 *   This library is distributed in the hope that it will be useful,          *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of           *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU        *
 *   Lesser General Public License for more details.                          *
 *                                                                            *
 *   You should have received a copy of the Lesser GNU General Public License *
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.    *
 *****************************************************************************/
#include <QtCore/QString>
#include <QtTest/QtTest>

#include "../src/lib/configurationmanager_interface_singleton.h"
#include "../src/lib/callmanager_interface_singleton.h"
#include "../src/lib/instance_interface_singleton.h"

class DBusTests: public QObject
{
   Q_OBJECT
private slots:
   void testConfigurationManagerConnection();
   void testCallManagerConnection();
   void testInstanceManagerConnection();
};

void DBusTests::testConfigurationManagerConnection()
{
   ConfigurationManagerInterface& configurationManager = ConfigurationManagerInterfaceSingleton::getInstance();
   QDBusReply<QStringList> audioPlugins = configurationManager.getAudioPluginList();
   QCOMPARE( audioPlugins.isValid(), true );
}

void DBusTests::testCallManagerConnection()
{
   CallManagerInterface& callManager = CallManagerInterfaceSingleton::getInstance();
   QDBusReply<QStringList> callList = callManager.getCallList();
   QCOMPARE( callList.isValid(), true );
}

void DBusTests::testInstanceManagerConnection()
{
   InstanceInterface& instance = InstanceInterfaceSingleton::getInstance();
   QDBusReply<void> ret = instance.Register(getpid(), "unitTest");
   instance.Unregister(getpid());
   QCOMPARE( ret.isValid(), true );
}

QTEST_MAIN(DBusTests)
#include "dbus_test.moc"
