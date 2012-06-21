#include <QString>
#include <QtTest>
#include <QtCore>

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