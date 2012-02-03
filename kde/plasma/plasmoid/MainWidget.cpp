#include "MainWidget.h"

#include "../../src/lib/sflphone_const.h"
#include "../../src/lib/callmanager_interface_singleton.h"
#include "../../src/lib/CallModel.h"

MainWidget::MainWidget() : Plasma::Frame(), m_mainLayout(0), frmCalls(0), frmContact(0), frmHistory(0),currentMode(CALL)
{
   mainTabs          = new Plasma::TabBar        ( this         );
   frmCalls          = new Plasma::Frame         ( this         );
   callLayout        = new QGraphicsLinearLayout ( Qt::Vertical );
   callScrollArea    = new Plasma::ScrollWidget  ( this         );
   frmContact        = new Plasma::Frame         ( this         );
   contactScrollArea = new Plasma::ScrollWidget  ( this         );
   frmHistory        = new Plasma::Frame         ( this         );
   historyLayout     = new QGraphicsLinearLayout ( Qt::Vertical );
   historyScrollArea = new Plasma::ScrollWidget  ( this         );

   mainLayout()->addItem(mainTabs);
   //connect(mainTabs, SIGNAL(currentChanged(int)), this, SLOT(modeChanged(int)));

   frmCalls->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
   frmCalls->setLayout(callLayout);

   callScrollArea->setWidget(frmCalls);

   frmContact->setText("Contact");
   frmContact->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

   contactScrollArea->setWidget(frmContact);

   frmHistory->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
   frmHistory->setLayout(historyLayout);

   historyScrollArea->setWidget(frmHistory);

   dialPage = new DialPage();
   connect(dialPage, SIGNAL(call(QString)), this, SLOT(call(QString)));

   QGraphicsLinearLayout* callTabLayout    = new QGraphicsLinearLayout( Qt::Vertical );
   QGraphicsLinearLayout* historyTabLayout = new QGraphicsLinearLayout( Qt::Vertical );
   QGraphicsLinearLayout* contactTabLayout = new QGraphicsLinearLayout( Qt::Vertical );
   QGraphicsLinearLayout* addTabLayout     = new QGraphicsLinearLayout( Qt::Vertical );

   callTabLayout->addItem    ( callScrollArea    );
   historyTabLayout->addItem ( historyScrollArea );
   contactTabLayout->addItem ( contactScrollArea );
   addTabLayout->addItem(dialPage);

   mainTabs->addTab( "Call"            , callTabLayout    );
   mainTabs->addTab( "History"         , historyTabLayout );
   mainTabs->addTab( "Contact"         , contactTabLayout );
   mainTabs->addTab( KIcon("list-add") , "", addTabLayout );

   //mainLayout()->addItem(callTabLayout);

   //mainLayout()->addItem(callScrollArea);
   //mainLayout()->addItem(historyScrollArea);
   //mainLayout()->addItem(contactScrollArea);

   initPos = frmCalls->pos();

   frmCalls->setPos  ( initPos );
   frmHistory->setPos( initPos );
   frmContact->setPos( initPos );

   setMinimumSize(285,390);
}

void MainWidget::dataUpdated(const QString& source, const Plasma::DataEngine::Data& data)
{
   if ((source == "calls") && (frmCalls)) {
      QHash<QString, QVariant> value = data;
      bool modified = false;
      foreach(QVariant call, value) {
         if (!callWidgetList[value.key(call)]) {
            callWidgetList[ value.key(call) ] = new CallItem();
            callWidgetList[ value.key(call) ]->setCallId(value.key(call));
            callLayout->insertItem(0,callWidgetList[value.key(call)]);
            mainTabs->setCurrentIndex(CALL);
            modified = true;
         }
         callWidgetList[value.key(call)]->setCallerName(call.toHash()  [ "Name"   ].toString());
         callWidgetList[value.key(call)]->setCallerNumber(call.toHash()[ "Number" ].toString());
         callWidgetList[value.key(call)]->setState(call.toHash()       [ "State"  ].toInt());

         if (call.toHash()["State"].toInt() == CALL_STATE_INCOMING) {
            emit requierAttention();
         }

         topLevelItems[value.key(call)] = callWidgetList[value.key(call)];
      }

//       if (modified)
//         sflphoneEngine->connectSource("conferences", this,0/*Update only if something happen*/);
   }
   else if (source == "info") {
      if (!data["Account"].isNull() || data["Account"].isValid())
         currentAccountId = data["Account"].toString();
   }
   else if (source == "conferences") {
      QHash<QString, QVariant> value = data;
      foreach(QVariant call, value) {
         qDebug() << "Painting conference: " << call;//.toHash();
         if (!topLevelItems[value.key(call)]) {
            topLevelItems[value.key(call)] = new ConferenceItem();
            ((ConferenceItem*) topLevelItems[value.key(call)])->setConfId(value.key(call));
            callLayout->addItem((ConferenceItem*) topLevelItems[value.key(call)]);
            mainTabs->setCurrentIndex(CALL);
         }
         QList<CallItem*> toSend;
         foreach (QString callId, call.toStringList()) {
            if (callWidgetList[callId]) {
               toSend.push_back(callWidgetList[callId]);
               callWidgetList[callId]->setParent(0);
               if (topLevelItems[callId]) {
                   topLevelItems.remove(callId);
               }
            }
            else
               qDebug() << "Call not found";
         }
         ((ConferenceItem*)topLevelItems[value.key(call)])->setCallList(toSend);
      }
   }
   else if (source == "history") {
      QHash<QString, QVariant> value = data;
      for (QHash<QString, QVariant>::iterator i = value.begin(); i != value.end(); ++i) {
         QString key = i.key();
         //QHash<QString, CallItem*> historyWidgetList;
         if ( ! historyWidgetList[key]) {
            historyWidgetList[ i.key() ] = new CallItem();
            historyWidgetList[ i.key() ]->setCallId(i.key());
            historyLayout->addItem(historyWidgetList[i.key()]);
         }
         historyWidgetList[ i.key() ]->setCallerName(i.value().toHash()   [ "Name"   ].toString());
         historyWidgetList[ i.key() ]->setCallerNumber(i.value().toHash() [ "Number" ].toString());
         historyWidgetList[ i.key() ]->setDate(i.value().toHash()         [ "Date"   ].toInt())   ;
      }
   }
}

QGraphicsLinearLayout* MainWidget::mainLayout()
{
   if (!m_mainLayout) {
      m_mainLayout = new QGraphicsLinearLayout(Qt::Vertical);
      m_mainLayout->setContentsMargins(0, 0, 0, 0);
      m_mainLayout->setSpacing(5);
      setLayout(m_mainLayout);
   }
   return m_mainLayout;
}

void MainWidget::call(QString number)
{
   Q_UNUSED(number);
//    qDebug() << "Calling " << number << " with account " << currentAccountId << ", " << CallModelConvenience::getAccountList()->size() << " account registred";
//    CallManagerInterface& callManager = CallManagerInterfaceSingleton::getInstance();
//    callManager.placeCall(currentAccountId, CallModelConvenience::generateCallId(), number);
}
