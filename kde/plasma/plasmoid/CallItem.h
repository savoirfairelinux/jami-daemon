#ifndef PLASMOID_CALL_ITEM
#define PLASMOID_CALL_ITEM

#include <Plasma/Svg>
#include <QGraphicsGridLayout>
#include <Plasma/Frame>
#include "ViewItem.h"

class CallItem : public Plasma::Frame, public ViewItem
{
   Q_OBJECT
   public:
      CallItem();
      QString getCallId();
      void setCallId(QString _callId);
      bool isChanged();
      void setCallerName(QString value);
      void setCallerNumber(QString value);
      void setState(int state);
      void setDate(int date);
      ViewItem* parent();
      void setParent(ViewItem* parent);
      bool isConference()
      {
         return false;
      }
   private:
      Plasma::Frame* frmCallerName;
      Plasma::Frame* frmCallerNumber;
      Plasma::Frame* frmDate;
      Plasma::Frame* frmStateIcon;
      bool changed;
      QString callId;
      ViewItem* parentItem;
      QGraphicsGridLayout* mainLayout;

};

#endif
