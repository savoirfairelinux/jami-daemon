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
   /*void testValidity();
   void testMonth_data();
   void testMonth();*/
   void testConfigurationManagerConnection();
   void testCallManagerConnection();
   void testInstanceManagerConnection();
};

/*void DBusTests::testValidity()
{
   QDate date( 1967, 3, 12 );
   QVERIFY( date.isValid() );
}

void DBusTests::testMonth_data()
{
   QTest::addColumn<int>("year");  // the year we are testing
   QTest::addColumn<int>("month"); // the month we are testing
   QTest::addColumn<int>("day");   // the day we are testing
   QTest::addColumn<QString>("monthName");   // the name of the month

   QTest::newRow("1967/3/11") << 1967 << 3 << 11 << QString("March");
   QTest::newRow("1966/1/10") << 1966 << 1 << 10 << QString("January");
   QTest::newRow("1999/9/19") << 1999 << 9 << 19 << QString("September");
   // more rows of dates can go in here...
}

void DBusTests::testMonth()
{
   QFETCH(int, year);
   QFETCH(int, month);
   QFETCH(int, day);
   QFETCH(QString, monthName);



   QDate date;
   date.setYMD( year, month, day);
   QCOMPARE( date.month(), month );
   QCOMPARE( QDate::longMonthName(date.month()), monthName );
}*/

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
#include "dlgaccount_test.moc"