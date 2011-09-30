#ifndef MAIN_WIDGET_H
#define MAIN_WIDGET_H

#include <Plasma/Frame>
#include <Plasma/Svg>
#include <QGraphicsLinearLayout>
#include <Plasma/Frame>
#include <plasma/theme.h>
#include <plasma/widgets/tabbar.h>
//#include <plasma/animations/animation.h>
#include <Plasma/ScrollWidget>
#include <Plasma/DataEngine>
#include <Plasma/ExtenderItem>
#include <KIcon>
#include <QHash>

#include "CallItem.h"
#include "ConferenceItem.h"
#include "DialPage.h"

#define CALL 0
#define HISTORY 1
#define CONTACT 2

class MainWidget : public Plasma::Frame
{
   Q_OBJECT
   public:
      MainWidget();
      void dataUpdated(const QString& source, const Plasma::DataEngine::Data &data);

   private:
      KIcon m_icon;
      QGraphicsLinearLayout* m_mainLayout;
      QGraphicsLinearLayout* mainLayout();
      Plasma::Frame* frmCalls;
      Plasma::Frame* frmContact;
      Plasma::Frame* frmHistory;
      QGraphicsLinearLayout* callLayout;
      QGraphicsLinearLayout* historyLayout;
      QHash<QString, CallItem*> callWidgetList;
      QHash<QString, ViewItem*> topLevelItems;
      QHash<QString, CallItem*> historyWidgetList;
      Plasma::ScrollWidget* callScrollArea;
      Plasma::ScrollWidget* historyScrollArea;
      Plasma::ScrollWidget* contactScrollArea;
      Plasma::ScrollWidget* newScrollArea;
      DialPage* dialPage;
      QString currentAccountId;
      Plasma::TabBar* mainTabs;
      QPointF initPos;
      int currentMode;
   private slots:
      void call(QString number);

   signals:
      void requierAttention();
};
#endif
