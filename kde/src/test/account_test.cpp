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

   //Attributes test
   void testAccountAlias                  ();
   void testAccountType                   ();
   void testAccountHostname               ();
   void testAccountUsername               ();
   void testAccountPassword               ();
   void testAccountMailbox                ();
   void testTlsPassword                   ();
   void testTlsCaListFile                 ();
   void testTlsCertificateFile            ();
   void testTlsPrivateKeyFile             ();
   void testTlsCiphers                    ();
   void testTlsServerName                 ();
   void testAccountSipStunServer          ();
   void testPublishedAddress              ();
   void testLocalInterface                ();
   void testConfigRingtonePath            ();
   void testRingtonePath                  ();
   void testTlsMethod                     ();
   void testAccountRegistrationExpire     ();
   void testTlsNegotiationTimeoutSec      ();
   void testTlsNegotiationTimeoutMsec     ();
   void testLocalPort                     ();
   void testTlsListenerPort               ();
   void testPublishedPort                 ();
   void testAccountEnabled                ();
   void testTlsVerifyServer               ();
   void testTlsVerifyClient               ();
   void testTlsRequireClientCertificate   ();
   void testTlsEnable                     ();
   void testAccountDisplaySasOnce         ();
   void testAccountSrtpRtpFallback        ();
   void testAccountZrtpDisplaySas         ();
   void testAccountZrtpNotSuppWarning     ();
   void testAccountZrtpHelloHash          ();
   void testAccountSipStunEnabled         ();
   void testPublishedSameAsLocal          ();
   void testConfigRingtoneEnabled         ();

private:
   QString id;
   
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
   Account* acc = AccountList::getInstance()->getAccountById(id);
   QCOMPARE( acc != nullptr, true);
}
//END creating a new account

//BEGIN Testing every account attributes

void AccountTests::testAccountAlias                  ()/*QString detail*/
{
   Account* acc = AccountList::getInstance()->getAccountById(id);
   acc->setAccountAlias("unit_alias");
   QCOMPARE( acc->getAccountAlias(), QString("unit_alias"));
}

void AccountTests::testAccountType                   ()/*QString detail*/
{
   Account* acc = AccountList::getInstance()->getAccountById(id);
   
}

void AccountTests::testAccountHostname               ()/*QString detail*/
{
   Account* acc = AccountList::getInstance()->getAccountById(id);
   
}

void AccountTests::testAccountUsername               ()/*QString detail*/
{
   Account* acc = AccountList::getInstance()->getAccountById(id);
   
}

void AccountTests::testAccountPassword               ()/*QString detail*/
{
   Account* acc = AccountList::getInstance()->getAccountById(id);
   
}

void AccountTests::testAccountMailbox                ()/*QString detail*/
{
   Account* acc = AccountList::getInstance()->getAccountById(id);
   
}

void AccountTests::testTlsPassword                   ()/*QString detail*/
{
   Account* acc = AccountList::getInstance()->getAccountById(id);
   
}

void AccountTests::testTlsCaListFile                 ()/*QString detail*/
{
   Account* acc = AccountList::getInstance()->getAccountById(id);
   
}

void AccountTests::testTlsCertificateFile            ()/*QString detail*/
{
   Account* acc = AccountList::getInstance()->getAccountById(id);
   
}

void AccountTests::testTlsPrivateKeyFile             ()/*QString detail*/
{
   Account* acc = AccountList::getInstance()->getAccountById(id);
   
}

void AccountTests::testTlsCiphers                    ()/*QString detail*/
{
   Account* acc = AccountList::getInstance()->getAccountById(id);
   
}

void AccountTests::testTlsServerName                 ()/*QString detail*/
{
   Account* acc = AccountList::getInstance()->getAccountById(id);
   
}

void AccountTests::testAccountSipStunServer          ()/*QString detail*/
{
   Account* acc = AccountList::getInstance()->getAccountById(id);
   
}

void AccountTests::testPublishedAddress              ()/*QString detail*/
{
   Account* acc = AccountList::getInstance()->getAccountById(id);
   
}

void AccountTests::testLocalInterface                ()/*QString detail*/
{
   Account* acc = AccountList::getInstance()->getAccountById(id);
   
}

void AccountTests::testConfigRingtonePath            ()/*QString detail*/
{
   Account* acc = AccountList::getInstance()->getAccountById(id);
   
}

void AccountTests::testRingtonePath                  ()/*QString detail*/
{
   Account* acc = AccountList::getInstance()->getAccountById(id);
   
}

void AccountTests::testTlsMethod                     ()/*int     detail*/
{
   Account* acc = AccountList::getInstance()->getAccountById(id);
   
}

void AccountTests::testAccountRegistrationExpire     ()/*int     detail*/
{
   Account* acc = AccountList::getInstance()->getAccountById(id);
   
}

void AccountTests::testTlsNegotiationTimeoutSec      ()/*int     detail*/
{
   Account* acc = AccountList::getInstance()->getAccountById(id);
   
}

void AccountTests::testTlsNegotiationTimeoutMsec     ()/*int     detail*/
{
   Account* acc = AccountList::getInstance()->getAccountById(id);
   
}

void AccountTests::testLocalPort                     ()/*short   detail*/
{
   Account* acc = AccountList::getInstance()->getAccountById(id);
   
}

void AccountTests::testTlsListenerPort               ()/*short   detail*/
{
   Account* acc = AccountList::getInstance()->getAccountById(id);
   
}

void AccountTests::testPublishedPort                 ()/*short   detail*/
{
   Account* acc = AccountList::getInstance()->getAccountById(id);
   
}

void AccountTests::testAccountEnabled                ()/*bool    detail*/
{
   Account* acc = AccountList::getInstance()->getAccountById(id);
   
}

void AccountTests::testTlsVerifyServer               ()/*bool    detail*/
{
   Account* acc = AccountList::getInstance()->getAccountById(id);
   
}

void AccountTests::testTlsVerifyClient               ()/*bool    detail*/
{
   Account* acc = AccountList::getInstance()->getAccountById(id);
   
}

void AccountTests::testTlsRequireClientCertificate   ()/*bool    detail*/
{
   Account* acc = AccountList::getInstance()->getAccountById(id);
   
}

void AccountTests::testTlsEnable                     ()/*bool    detail*/
{
   Account* acc = AccountList::getInstance()->getAccountById(id);
   
}

void AccountTests::testAccountDisplaySasOnce         ()/*bool    detail*/
{
   Account* acc = AccountList::getInstance()->getAccountById(id);
   
}

void AccountTests::testAccountSrtpRtpFallback        ()/*bool    detail*/
{
   Account* acc = AccountList::getInstance()->getAccountById(id);
   
}

void AccountTests::testAccountZrtpDisplaySas         ()/*bool    detail*/
{
   Account* acc = AccountList::getInstance()->getAccountById(id);
   
}

void AccountTests::testAccountZrtpNotSuppWarning     ()/*bool    detail*/
{
   Account* acc = AccountList::getInstance()->getAccountById(id);
   
}

void AccountTests::testAccountZrtpHelloHash          ()/*bool    detail*/
{
   Account* acc = AccountList::getInstance()->getAccountById(id);
   
}

void AccountTests::testAccountSipStunEnabled         ()/*bool    detail*/
{
   Account* acc = AccountList::getInstance()->getAccountById(id);
   
}

void AccountTests::testPublishedSameAsLocal          ()/*bool    detail*/
{
   Account* acc = AccountList::getInstance()->getAccountById(id);
   
}

void AccountTests::testConfigRingtoneEnabled         ()/*bool    detail*/
{
   Account* acc = AccountList::getInstance()->getAccountById(id);
   
}

//END Testing every account attributes

//BEGIN cleanup
void AccountTests::cleanupTestCase() {
   //AccountList::getInstance()->removeAccount(AccountList::getInstance()->getAccountById(id));
   //QCOMPARE( AccountList::getInstance()->getAccountById(id) == nullptr, true);
}
//END cleanup

QTEST_MAIN(AccountTests)
#include "account_test.moc"