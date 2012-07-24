#include <QtCore/QString>
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
   QMap<Account*,bool> saveState;
   //Disable all accounts
   for (int i=0;i<AccountList::getInstance()->size();i++) {
      saveState[(*AccountList::getInstance())[i]] = (*AccountList::getInstance())[i]->isAccountEnabled();
      qDebug() << "Disabling" << (*AccountList::getInstance())[i]->getAccountId();
      (*AccountList::getInstance())[i]->setAccountEnabled(false);
      (*AccountList::getInstance())[i]->save();
   }

    Call* call = m_pModel->addDialingCall("test call", AccountList::getCurrentAccount());
    QCOMPARE( call, (Call*)nullptr );

   //Restore state
   for (int i=0;i<AccountList::getInstance()->size();i++) {
      (*AccountList::getInstance())[i]->setAccountEnabled(saveState[(*AccountList::getInstance())[i]]);
      (*AccountList::getInstance())[i]->save();
   }
}

QTEST_MAIN(CallTests)
#include "call_test.moc"
