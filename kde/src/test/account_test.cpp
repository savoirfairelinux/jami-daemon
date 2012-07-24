#include <QtCore/QString>
#include <QtTest/QtTest>
//#include <QtCore>

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
   void testAccountAlias_data();
   void testAccountAlias                  ();
   
   void testAccountType                   ();
   void testAccountHostname_data();
   void testAccountHostname               ();
   void testAccountHostnameInvalid_data   ();
   void testAccountHostnameInvalid        ();
   void testAccountUsername               ();
   void testAccountPassword_data          ();
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
   void testRingtonePath_data();
   void testRingtonePath                  ();
   void testTlsMethod                     ();
   void testAccountRegistrationExpire     ();
   void testTlsNegotiationTimeoutSec      ();
   void testTlsNegotiationTimeoutMsec     ();
   void testLocalPort_data();
   void testLocalPort                     ();
   void testTlsListenerPort_data();
   void testTlsListenerPort               ();
   void testPublishedPort_data();
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
   void testDisableAllAccounts            ();

private:
   QString id;
   
};


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
   id = acc->getAccountId();
   QCOMPARE( acc != nullptr, true);
}

void AccountTests::testGetNewAccount()
{
   Account* acc = AccountList::getInstance()->getAccountById(id);
   QCOMPARE( acc != nullptr, true);
}
//END creating a new account

//BEGIN Testing every account attributes

void AccountTests::testAccountAlias_data()
{
   QTest::addColumn<QString>("alias");

   QTest::newRow("valid"      ) << QString( "unit_alias" );
   QTest::newRow("valid_reset") << QString( "unit_alias1");
   QTest::newRow("numeric"    ) << QString( "2314234"    );
   QTest::newRow("non-ascii"  ) << QString( "ééèè>>||``" );
}

void AccountTests::testAccountAlias                  ()/*QString detail*/
{
   Account* acc = AccountList::getInstance()->getAccountById(id);
   //acc->setAccountAlias("unit_alias");
   //QCOMPARE( acc->getAccountAlias(), QString("unit_alias"));

   QFETCH(QString, alias);
   acc->setAccountAlias(alias);
   acc->save();
   QCOMPARE( acc->getAccountAlias(), alias );
   
}

void AccountTests::testAccountType                   ()/*QString detail*/
{
   Account* acc = AccountList::getInstance()->getAccountById(id);
   acc->setAccountType("IAX");
   acc->save();
   QCOMPARE( acc->getAccountType(), QString("IAX") );
   acc->setAccountType("SIP");
   acc->save();
   QCOMPARE( acc->getAccountType(), QString("SIP") );

   //Test invalid
   acc->setAccountType("OTH");
   QCOMPARE( acc->getAccountType() == "OTH", false );
}

void AccountTests::testAccountHostname_data()
{
   QTest::addColumn<QString>("hostname");
   QTest::newRow("valid"          ) << QString( "validHostname"  );
   QTest::newRow("validPlusDigit" ) << QString( "validHostname2" );
   QTest::newRow("ipv4"           ) << QString( "192.168.77.12"  );
   QTest::newRow("ipv42"          ) << QString( "10.0.250.1"     );
}

//This test the various hostnames that should be allowed by the daemon
void AccountTests::testAccountHostname               ()
{
   Account* acc = AccountList::getInstance()->getAccountById(id);
   QFETCH(QString, hostname);
   acc->setAccountHostname(hostname);
   acc->save();
   QCOMPARE( acc->getAccountHostname(), hostname );
}

void AccountTests::testAccountHostnameInvalid_data()
{
   QTest::addColumn<QString>("hostname");
   QTest::newRow("invalid_ipv4"       ) << QString( "192.256.12.0"    );
   QTest::newRow("invalid_ipv42"      ) << QString( "192.168.12.0000" );
   QTest::newRow("invalid_ipv43"      ) << QString( "192.168.12."     );
   QTest::newRow("invalid_ipv44"      ) << QString( "192.168.1"       );
   QTest::newRow("invalid_ipv45"      ) << QString( ".192.168.1.1"    );
   QTest::newRow("invalid_ipv46"      ) << QString( ".192.1E8.1.1"    );
   QTest::newRow("invalid_hostname"   ) << QString( ".23423"          );
}

//This test hostname that should be rejected by the daemon
void AccountTests::testAccountHostnameInvalid        ()
{
   QFETCH(QString, hostname);
   Account* acc = AccountList::getInstance()->getAccountById(id);
   acc->setAccountHostname(hostname);
   acc->save();
   QVERIFY(acc->getAccountHostname() != hostname);
}

void AccountTests::testAccountUsername               ()/*QString detail*/
{
   Account* acc = AccountList::getInstance()->getAccountById(id);
   acc->setAccountHostname("1234567879");
   acc->save();
   QString username = acc->getAccountHostname();
   QCOMPARE(username,QString("1234567879"));
   
}

void AccountTests::testAccountPassword_data()
{
   QTest::addColumn<QString>("password");
   QTest::newRow( "numeric"      ) << QString( "1234567879"   );
   QTest::newRow( "alphanumeric" ) << QString( "asdf1234"     );
   QTest::newRow( "strong"       ) << QString( "!\"'''4)(--@" );
}

void AccountTests::testAccountPassword               ()/*QString detail*/
{
   /*QFETCH(QString, password);
   Account* acc = AccountList::getInstance()->getAccountById(id);
   acc->setAccountPassword(password);
   QString pwd = acc->getAccountPassword();
   QCOMPARE(pwd,password);*/
}

void AccountTests::testAccountMailbox                ()/*QString detail*/
{
   Account* acc = AccountList::getInstance()->getAccountById(id);
   acc->setAccountMailbox("1234567879");
   acc->save();
   QString mailbox = acc->getAccountMailbox();
   QCOMPARE(mailbox,QString("1234567879"));
   
}

void AccountTests::testTlsPassword                   ()/*QString detail*/
{
   Account* acc = AccountList::getInstance()->getAccountById(id);
   acc->setTlsPassword("1234567879");
   acc->save();
   QString tlspass = acc->getTlsPassword();
   QCOMPARE(tlspass,QString("1234567879"));
}

void AccountTests::testTlsCaListFile                 ()/*QString detail*/
{
   //Account* acc = AccountList::getInstance()->getAccountById(id);
   QSKIP("TODO",SkipAll);
}

void AccountTests::testTlsCertificateFile            ()/*QString detail*/
{
   //Account* acc = AccountList::getInstance()->getAccountById(id);
   QSKIP("TODO",SkipAll);
}

void AccountTests::testTlsPrivateKeyFile             ()/*QString detail*/
{
   //Account* acc = AccountList::getInstance()->getAccountById(id);
   QSKIP("TODO",SkipAll);
}

void AccountTests::testTlsCiphers                    ()/*QString detail*/
{
   //Account* acc = AccountList::getInstance()->getAccountById(id);
   QSKIP("TODO",SkipAll);
}

void AccountTests::testTlsServerName                 ()/*QString detail*/
{
   Account* acc = AccountList::getInstance()->getAccountById(id);
   acc->setTlsServerName("qwerty");
   acc->save();
   QString tlsserver = acc->getTlsServerName();
   QCOMPARE(tlsserver,QString("qwerty"));
}

void AccountTests::testAccountSipStunServer          ()/*QString detail*/
{
   Account* acc = AccountList::getInstance()->getAccountById(id);
   acc->setAccountSipStunServer("qwerty");
   acc->save();
   QString tlsserver = acc->getAccountSipStunServer();
   QCOMPARE(tlsserver,QString("qwerty"));
}

void AccountTests::testPublishedAddress              ()/*QString detail*/
{
   //Account* acc = AccountList::getInstance()->getAccountById(id);
   QSKIP("TODO",SkipAll);
}

void AccountTests::testLocalInterface                ()/*QString detail*/
{
   //Account* acc = AccountList::getInstance()->getAccountById(id);
   QSKIP("TODO",SkipAll);
}

void AccountTests::testRingtonePath_data()
{
   QTest::addColumn<QString>("path");
   QMap<QString,QString> ringtonePaths = ConfigurationManagerInterfaceSingleton::getInstance().getRingtoneList();
   QMutableMapIterator<QString, QString> iter(ringtonePaths);
   while (iter.hasNext()) {
      iter.next();
      QTest::newRow( iter.value().toAscii() ) << iter.key();
   }
   QTest::newRow( "invalidWav" ) << QString("invalid/tmp2/fake_file.wav");
   QTest::newRow( "invalidAu"  ) << QString("invalid/tmp2/fake_file.au");
   QTest::newRow( "invalidUl"  ) << QString("invalid/tmp2/fake_file.ul");
   
   QString tmpPath = QDir::tempPath();
   QFile file (tmpPath+"/testWav.wav");
   file.open(QIODevice::WriteOnly);
   file.close();
   QTest::newRow( "wav_file"      ) << tmpPath+"/testWav.wav";
   
   QFile file2 (tmpPath+"/testAu.au");
   file2.open(QIODevice::WriteOnly);
   file2.close();
   QTest::newRow( "au_file"      ) << tmpPath+"/testAu.au";

   QFile file3 (tmpPath+"/testUl.ul");
   file3.open(QIODevice::WriteOnly);
   file3.close();
   QTest::newRow( "ul_file"      ) << tmpPath+"/testUl.ul";
   
}

void AccountTests::testRingtonePath                  ()/*QString detail*/
{
   Account* acc = AccountList::getInstance()->getAccountById(id);
   QFETCH(QString, path);
   acc->setRingtonePath(path);
   acc->save();
   if (path.indexOf("invalid") != -1)
      QCOMPARE(acc->getRingtonePath() == path ,false);
   else
      QCOMPARE(acc->getRingtonePath(),path);
}

void AccountTests::testTlsMethod                     ()/*int     detail*/
{
   //Account* acc = AccountList::getInstance()->getAccountById(id);
   QSKIP("TODO",SkipAll);
}

void AccountTests::testAccountRegistrationExpire     ()/*int     detail*/
{
   Account* acc = AccountList::getInstance()->getAccountById(id);
   acc->setAccountRegistrationExpire(10000);
   acc->save();
   QCOMPARE(acc->getAccountRegistrationExpire(),10000);

   //Time machines are not on the market yet
   acc->setAccountRegistrationExpire(-10000);
   acc->save();
   QCOMPARE(acc->getAccountRegistrationExpire() == -10000,false);
}

void AccountTests::testTlsNegotiationTimeoutSec      ()/*int     detail*/
{
   Account* acc = AccountList::getInstance()->getAccountById(id);
   acc->setTlsNegotiationTimeoutSec(10000);
   acc->save();

   QCOMPARE(acc->getTlsNegotiationTimeoutSec(),10000);

   //Time machines are not on the market yet
   acc->setTlsNegotiationTimeoutSec(-10000);
   acc->save();
   QCOMPARE(acc->getTlsNegotiationTimeoutSec() == -10000,false);
}

void AccountTests::testTlsNegotiationTimeoutMsec     ()/*int     detail*/
{
   Account* acc = AccountList::getInstance()->getAccountById(id);

   acc->setTlsNegotiationTimeoutMsec(10000);
   acc->save();
   QCOMPARE(acc->getTlsNegotiationTimeoutMsec(),10000);

   //Time machines are not on the market yet
   acc->setTlsNegotiationTimeoutMsec(-10000);
   acc->save();
   QCOMPARE(acc->getTlsNegotiationTimeoutMsec() == -10000,false);
}

void AccountTests::testLocalPort_data()
{
   //It is really an unsigned short, but lets do real tests instead
   QTest::addColumn<int>("port");
   QTest::newRow( "null"     ) << 0      ;
   QTest::newRow( "2000"     ) << 2000   ;
   QTest::newRow( "high"     ) << 65533  ;
   QTest::newRow( "over"     ) << 1000000;
   QTest::newRow( "negative" ) << -1000  ;

}

void AccountTests::testLocalPort                     ()/*short   detail*/
{
   Account* acc = AccountList::getInstance()->getAccountById(id);
   QFETCH(int, port);
   acc->setLocalPort(port);
   acc->save();
   if (port < 0 || port > 65533)
      QCOMPARE(acc->getLocalPort() == port,false);
   else
      QCOMPARE(acc->getLocalPort(),port);
}

void AccountTests::testTlsListenerPort_data()
{
   //It is really an unsigned short, but lets do real tests instead
   QTest::addColumn<int>("port");
   QTest::newRow( "null"     ) << 0      ;
   QTest::newRow( "2000"     ) << 2000   ;
   QTest::newRow( "high"     ) << 65533  ;
   QTest::newRow( "over"     ) << 1000000;
   QTest::newRow( "negative" ) << -1000  ;

}

void AccountTests::testTlsListenerPort               ()/*short   detail*/
{
   Account* acc = AccountList::getInstance()->getAccountById(id);
   QFETCH(int, port);
   acc->setTlsListenerPort(port);
   acc->save();
   if (port < 0 || port > 65533)
      QCOMPARE(acc->getTlsListenerPort() == port,false);
   else
      QCOMPARE(acc->getTlsListenerPort(),port);
}

void AccountTests::testPublishedPort_data()
{
   //It is really an unsigned short, but lets do real tests instead
   QTest::addColumn<int>("port");
   QTest::newRow( "null"     ) << 0      ;
   QTest::newRow( "2000"     ) << 2000   ;
   QTest::newRow( "high"     ) << 65533  ;
   QTest::newRow( "over"     ) << 1000000;
   QTest::newRow( "negative" ) << -1000  ;

}

void AccountTests::testPublishedPort                 ()/*short   detail*/
{
   Account* acc = AccountList::getInstance()->getAccountById(id);
   QFETCH(int, port);
   acc->setPublishedPort(port);
   acc->save();
   if (port < 0 || port > 65533)
      QCOMPARE(acc->getPublishedPort() == port,false);
   else
      QCOMPARE(acc->getPublishedPort(),port);
}

void AccountTests::testAccountEnabled                ()/*bool    detail*/
{
   Account* acc = AccountList::getInstance()->getAccountById(id);
   acc->setAccountEnabled(false);
   QCOMPARE(acc->isAccountEnabled(),false);
   acc->setAccountEnabled(true);
   QCOMPARE(acc->isAccountEnabled(),true);
}

void AccountTests::testTlsVerifyServer               ()/*bool    detail*/
{
   //Account* acc = AccountList::getInstance()->getAccountById(id);
   QSKIP("TODO",SkipAll);
}

void AccountTests::testTlsVerifyClient               ()/*bool    detail*/
{
   //Account* acc = AccountList::getInstance()->getAccountById(id);
   QSKIP("TODO",SkipAll);
}

void AccountTests::testTlsRequireClientCertificate   ()/*bool    detail*/
{
   Account* acc = AccountList::getInstance()->getAccountById(id);
   acc->setTlsRequireClientCertificate(true);
   acc->save();
   QCOMPARE(acc->isTlsRequireClientCertificate(),true);
   acc->setTlsRequireClientCertificate(false);
   acc->save();
   QCOMPARE(acc->isTlsRequireClientCertificate(),false);
}

void AccountTests::testTlsEnable                     ()/*bool    detail*/
{
   Account* acc = AccountList::getInstance()->getAccountById(id);
   acc->setTlsEnable(true);
   acc->save();
   QCOMPARE(acc->isTlsEnable(),true);
   acc->setTlsEnable(false);
   acc->save();
   QCOMPARE(acc->isTlsEnable(),false);
}

void AccountTests::testAccountDisplaySasOnce         ()/*bool    detail*/
{
   Account* acc = AccountList::getInstance()->getAccountById(id);
   acc->setAccountDisplaySasOnce(true);
   acc->save();
   QCOMPARE(acc->isAccountDisplaySasOnce(),true);
   acc->setAccountDisplaySasOnce(false);
   acc->save();
   QCOMPARE(acc->isAccountDisplaySasOnce(),false);
}

void AccountTests::testAccountSrtpRtpFallback        ()/*bool    detail*/
{
   Account* acc = AccountList::getInstance()->getAccountById(id);
   acc->setAccountSrtpRtpFallback(true);
   acc->save();
   QCOMPARE(acc->isAccountSrtpRtpFallback(),true);
   acc->setAccountSrtpRtpFallback(false);
   acc->save();
   QCOMPARE(acc->isAccountSrtpRtpFallback(),false);
}

void AccountTests::testAccountZrtpDisplaySas         ()/*bool    detail*/
{
   Account* acc = AccountList::getInstance()->getAccountById(id);
   acc->setAccountSrtpRtpFallback(true);
   acc->save();
   QCOMPARE(acc->isAccountSrtpRtpFallback(),true);
   acc->setAccountSrtpRtpFallback(false);
   acc->save();
   QCOMPARE(acc->isAccountSrtpRtpFallback(),false);
}

void AccountTests::testAccountZrtpNotSuppWarning     ()/*bool    detail*/
{
   Account* acc = AccountList::getInstance()->getAccountById(id);
   acc->setAccountZrtpNotSuppWarning(true);
   acc->save();
   QCOMPARE(acc->isAccountZrtpNotSuppWarning(),true);
   acc->setAccountZrtpNotSuppWarning(false);
   acc->save();
   QCOMPARE(acc->isAccountZrtpNotSuppWarning(),false);
}

void AccountTests::testAccountZrtpHelloHash          ()/*bool    detail*/
{
   Account* acc = AccountList::getInstance()->getAccountById(id);
   acc->setAccountZrtpHelloHash(true);
   acc->save();
   QCOMPARE(acc->isAccountZrtpHelloHash(),true);
   acc->setAccountZrtpHelloHash(false);
   acc->save();
   QCOMPARE(acc->isAccountZrtpHelloHash(),false);
}

void AccountTests::testAccountSipStunEnabled         ()/*bool    detail*/
{
   Account* acc = AccountList::getInstance()->getAccountById(id);
   acc->setAccountSipStunEnabled(true);
   acc->save();
   QCOMPARE(acc->isAccountSipStunEnabled(),true);
   acc->setAccountSipStunEnabled(false);
   acc->save();
   QCOMPARE(acc->isAccountSipStunEnabled(),false);
}

void AccountTests::testPublishedSameAsLocal          ()/*bool    detail*/
{
   Account* acc = AccountList::getInstance()->getAccountById(id);
   acc->setPublishedSameAsLocal(true);
   acc->save();
   QCOMPARE(acc->isPublishedSameAsLocal(),true);
   acc->setPublishedSameAsLocal(false);
   acc->save();
   QCOMPARE(acc->isPublishedSameAsLocal(),false);
}

void AccountTests::testConfigRingtoneEnabled         ()/*bool    detail*/
{
   Account* acc = AccountList::getInstance()->getAccountById(id);
   acc->setRingtoneEnabled(true);
   acc->save();
   QCOMPARE(acc->isRingtoneEnabled(),true);
   acc->setRingtoneEnabled(false);
   acc->save();
   QCOMPARE(acc->isRingtoneEnabled(),false);
}

//END Testing every account attributes

//BEGIN testing account list

void AccountTests::testDisableAllAccounts()
{
   QList<bool> saveState;
   //Disable all accounts
   for (int i=0;i<AccountList::getInstance()->size();i++) {
      saveState << (*AccountList::getInstance())[i]->isAccountEnabled();
      (*AccountList::getInstance())[i]->setAccountEnabled(false);
      (*AccountList::getInstance())[i]->save();
   }

   QCOMPARE(AccountList::getCurrentAccount(),(Account*)nullptr);

   //Restore state
   for (int i=0;i<AccountList::getInstance()->size();i++) {
      (*AccountList::getInstance())[i]->setAccountEnabled(saveState[i]);
      (*AccountList::getInstance())[i]->save();
   }
}

//END testing account list

//BEGIN cleanup
void AccountTests::cleanupTestCase() {
   AccountList::getInstance()->removeAccount(AccountList::getInstance()->getAccountById(id));
   QCOMPARE( AccountList::getInstance()->getAccountById(id) == nullptr, true);
}
//END cleanup

QTEST_MAIN(AccountTests)
#include "account_test.moc"
