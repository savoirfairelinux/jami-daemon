#ifndef SFLPHONE_PLASMOID_HEADER
#define SFLPHONE_PLASMOID_HEADER

#include <KIcon>

#include <Plasma/PopupApplet>
#include <Plasma/Svg>
#include <QGraphicsLinearLayout>
#include <Plasma/Frame>
#include <plasma/theme.h>
#include <plasma/widgets/tabbar.h>
//#include <plasma/animations/animation.h>
//#include <plasma/animator.h>
#include <Plasma/DataEngine>
#include <Plasma/ScrollWidget>
#include <Plasma/SvgWidget>
#include <Plasma/Extender>

#include "CallItem.h"
#include "ConferenceItem.h"
#include "DialPage.h"
#include "MainWidget.h"

class QSizeF;

enum FormFactor { Panel, Desktop };

class SFLPhonePlasmoid : public Plasma::PopupApplet
{
   Q_OBJECT
   public:
      SFLPhonePlasmoid(QObject* parent, const QVariantList &args);
      ~SFLPhonePlasmoid();
      void init();

   private:
      Plasma::Svg panel_icon;
      //Plasma::SvgWidget* panelFrame;
      Plasma::PushButton* panelFrame;

      Qt::Orientation m_orientation;
      void constraintsEvent(Plasma::Constraints constraints);
      Plasma::ExtenderItem* extenderItem;
      //void switchPage(Plasma::ScrollWidget* oldFrame, Plasma::ScrollWidget* newFrame, Plasma::ScrollWidget* toHide, bool direction);
      FormFactor formFaction;
      Plasma::DataEngine* contactEngine;
      Plasma::DataEngine* sflphoneEngine;
      MainWidget* mainWidget;
      bool callInit;
      bool requierAttention;
   private slots:
      //void modeChanged(int index);
      void showCallPopup(/*Qt::MouseButton button*/);
      void notify();

   public slots:
      void dataUpdated(const QString& source, const Plasma::DataEngine::Data &data);
};

// This is the command that links your applet to the .desktop file


#endif
