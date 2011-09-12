#include "CallItem.h"

#include <KStandardDirs>
#include <QDebug>
#include <time.h>

#include "../../src/lib/sflphone_const.h"

CallItem::CallItem() : frmDate(0), parentItem(0)
{
   setMinimumSize(QSizeF(24,64));
   setMaximumSize(QSizeF(10000000,64));
   
   frmCallerName = new Plasma::Frame(this);
   frmCallerNumber = new Plasma::Frame(this);
   frmStateIcon = new Plasma::Frame(this);
   frmStateIcon->setMinimumSize(QSizeF(24,24));
   frmStateIcon->setMaximumSize(QSizeF(24,24));
   changed = false;

   mainLayout = new QGraphicsGridLayout();
   mainLayout->setContentsMargins(0, 0, 0, 0);
   mainLayout->setSpacing(5);
   setLayout(mainLayout);

   //Plasma::Frame* infoFrame = new Plasma::Frame(this);

   //QGraphicsLinearLayout* infoLayout = new QGraphicsLinearLayout(Qt::Vertical);
   //infoLayout->setContentsMargins(0, 0, 0, 0);
   //infoLayout->setSpacing(5);
   //infoFrame->setLayout(infoLayout);
   mainLayout->addItem(frmStateIcon,0,0,2,1);
   mainLayout->addItem(frmCallerName,0,1);
   mainLayout->addItem(frmCallerNumber,1,1);

   
}

QString CallItem::getCallId()
{
   return callId;
}

void CallItem::setCallId(QString _callId)
{
   this->callId = _callId;
}

bool CallItem::isChanged()
{
   return changed;
}

void CallItem::setCallerName(QString value)
{
  frmCallerName->setText(value);
}

void CallItem::setCallerNumber(QString value)
{
   frmCallerNumber->setText(value);
}

void CallItem::setState(int state)
{
qDebug() << "State:" << KStandardDirs::locate("data", "icons/hold.svg");
   if (state == CALL_STATE_INCOMING) {
      frmStateIcon->setImage("/usr/share/kde4/apps/sflphone-plasmoid/icons/incoming.svg"/*KStandardDirs::locate("appdata", "icons/incoming.svg")*/);
   } else if (state == CALL_STATE_RINGING) {
      frmStateIcon->setImage("/usr/share/kde4/apps/sflphone-plasmoid/icons/outgoing.svg"/*KStandardDirs::locate("appdata", "icons/outgoing.svg")*/);
   } else if (state == CALL_STATE_CURRENT) {
      frmStateIcon->setImage("/usr/share/kde4/apps/sflphone-plasmoid/icons/call.svg"/*KStandardDirs::locate("appdata", "icons/call.svg")*/);
   } else if (state == CALL_STATE_DIALING) {
      frmStateIcon->setImage("/usr/share/kde4/apps/sflphone-plasmoid/icons/dial.svg"/*KStandardDirs::locate("appdata", "icons/dial.svg")*/);
   } else if (state == CALL_STATE_HOLD) {
      frmStateIcon->setImage("/usr/share/kde4/apps/sflphone-plasmoid/icons/hold.svg"/*KStandardDirs::locate("appdata", "icons/hold.svg")*/);
   } else if (state == CALL_STATE_FAILURE) {
      frmStateIcon->setImage("/usr/share/kde4/apps/sflphone-plasmoid/icons/fail.svg"/*KStandardDirs::locate("appdata", "icons/fail.svg")*/);
   } else if (state == CALL_STATE_BUSY) {
      frmStateIcon->setImage("/usr/share/kde4/apps/sflphone-plasmoid/icons/busy.svg"/*KStandardDirs::locate("appdata", "icons/busy.svg")*/);
   } else if (state == CALL_STATE_TRANSFER) {
      frmStateIcon->setImage("/usr/share/kde4/apps/sflphone-plasmoid/icons/transfert.svg"/*KStandardDirs::locate("appdata", "icons/transfert.svg")*/);
   } else if (state == CALL_STATE_TRANSF_HOLD) {
      frmStateIcon->setImage("/usr/share/kde4/apps/sflphone-plasmoid/icons/hold.svg"/*KStandardDirs::locate("appdata", "icons/hold.svg")*/);
   } else if (state == CALL_STATE_OVER) {
      frmStateIcon->setImage("/usr/share/kde4/apps/sflphone-plasmoid/icons/hang_up.svg"/*KStandardDirs::locate("appdata", "icons/hang_up.svg")*/);
   } else if (state == CALL_STATE_ERROR) {
      frmStateIcon->setImage("/usr/share/kde4/apps/sflphone-plasmoid/icons/fail.svg"/*KStandardDirs::locate("appdata", "icons/fail.svg")*/);
   }
}

ViewItem* CallItem::parent()
{
   return parentItem;
}

void CallItem::setParent(ViewItem* parent)
{
   parentItem = parent;
}

void CallItem::setDate(int date)
{
   if (!frmDate) {
      frmDate = new Plasma::Frame(this);
      mainLayout->addItem(frmDate,2,1);
   }
   time_t date2 = date;
   frmDate->setText(asctime(gmtime(&date2)));
}
