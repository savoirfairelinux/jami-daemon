#ifndef CONFERENCE_ITEM_H
#define CONFERENCE_ITEM_H

#include <Plasma/Frame>
#include <Plasma/FrameSvg>
#include <QGraphicsLinearLayout>

#include "ViewItem.h"
#include "CallItem.h"

class ConferenceItem : public Plasma::Frame, public ViewItem
{
   public:
      ConferenceItem();
      bool isConference()
      {
         return true;
      }
      void setCallList(QList<CallItem*> list);
      void setConfId(QString value);

   private:
      QString confId;
      QGraphicsLinearLayout* conferenceLayout;
      Plasma::Frame* header;
};

#endif
