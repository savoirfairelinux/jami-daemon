#include <QString>
#include <QtTest>
#include <QtCore>

#include "../src/lib/configurationmanager_interface_singleton.h"
#include "../src/lib/callmanager_interface_singleton.h"
#include "../src/lib/instance_interface_singleton.h"
#include "../src/lib/typedefs.h"
#include "../src/lib/Account.h"
#include "../src/lib/AccountList.h"
#include "../src/lib/dbus/metatypes.h"

class AccountTests: public QObject
{
   Q_OBJECT
private slots:
   /*void testValidity();
   void testMonth_data();
   void testMonth();*/

   //Testing accountlist
   void testAccountList();
   void testIP2IP();
   void testIP2IPAlias();

   //Building account
   void testCreateAccount();
   void testGetNewAccount();
   void cleanupTestCase();
   
};

/*void AccountTests::testValidity()
{
   QDate date( 1967, 3, 12 );
   QVERIFY( date.isValid() );
}

void AccountTests::testMonth_data()
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

void AccountTests::testMonth()
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

//BEGIN Getting a valid account
void AccountTests::testAccountList()
{
   AccountList* list = AccountList::getInstance();
   QCOMPARE( list != nullptr , true);
   QCOMPARE( list->size() >= 1, true);
}

void AccountTests::testIP2IP() {
   Account* acc = AccountList::getInstance()->getAccountById("IP2IP");
   QCOMPARE( acc != nullptr, true);
   if (acc) {
      QCOMPARE( acc->getAlias(), QString("IP2IP"));
   }
}

void AccountTests::testIP2IPAlias() {
   Account* acc = AccountList::getInstance()->getAccountById("IP2IP");
   if (acc) {
      QCOMPARE( acc->getAlias(), QString("IP2IP"));
      QCOMPARE( acc->getAlias() != QString("qwerty"), true);
   }
}
//END Getting a valid account


//BEGIN Creating a new account
void AccountTests::testCreateAccount()
{
   Account* acc = Account::buildNewAccountFromAlias("unit_test_account");
   acc->save();
   QCOMPARE( acc != nullptr, true);
}

void AccountTests::testGetNewAccount()
{
   Account* acc = AccountList::getInstance()->getAccountById("unit_test_account");
   QCOMPARE( acc != nullptr, true);
}
//END creating a new account


//BEGIN cleanup
void AccountTests::cleanupTestCase() {
   //AccountList::getInstance()->removeAccount(AccountList::getInstance()->getAccountById("unit_test_account"));
   //QCOMPARE( AccountList::getInstance()->getAccountById("unit_test_account") == nullptr, true);
}
//END cleanup

QTEST_MAIN(AccountTests)
#include "account_test.moc"