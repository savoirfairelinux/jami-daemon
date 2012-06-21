#include <QString>
#include <QtTest>
#include <QtCore>

#include "../src/lib/configurationmanager_interface_singleton.h"
#include "../src/lib/callmanager_interface_singleton.h"
#include "../src/lib/instance_interface_singleton.h"
#include "../src/lib/AccountList.h"
#include "../src/lib/CallModel.h"

CallModel<>* m_pModel = new CallModel<>();

class CallTests: public QObject
{
   Q_OBJECT
private slots:
   void testCallWithoutAccounts();
private:
   
};

///When there is no accounts, no call should be created
void CallTests::testCallWithoutAccounts()
{
   for (int i=0;i<AccountList::getInstance()->size();i++) {
      (*AccountList::getInstance())[i]->setAccountEnabled(false);
   }
   
    Call* call = m_pModel->addDialingCall("test call", AccountList::getCurrentAccount());
    QCOMPARE( call, (Call*)NULL );
}

QTEST_MAIN(CallTests)
#include "call_test.moc"