#include "ConferenceItem.h"

ConferenceItem::ConferenceItem() : Plasma::Frame()
{
   setEnabledBorders(Plasma::FrameSvg::AllBorders);
   setFrameShadow(Plasma::Frame::Sunken);
   //setStyleSheet("background-color:red;");

   header = new Plasma::Frame(this);
   header->setText("Conference");

   conferenceLayout = new QGraphicsLinearLayout(Qt::Vertical);

   conferenceLayout->addItem(header);
   
   setLayout(conferenceLayout);
}

void ConferenceItem::setCallList(QList<CallItem*> list)
{
   for (int i=0; i < conferenceLayout->count(); i++) {
      if (conferenceLayout->itemAt(i) != header) {
         CallItem* item = (CallItem*) conferenceLayout->itemAt(i);
         conferenceLayout->removeAt(i);
         item->setParent(0);
      }
   }

   foreach (CallItem* item, list) {
      conferenceLayout->addItem(item);
      item->setParent(this);
   }
   header->setText("Conference (" + QString::number(list.count())+")");
}

void ConferenceItem::setConfId(QString value)
{
   confId = value;
}
