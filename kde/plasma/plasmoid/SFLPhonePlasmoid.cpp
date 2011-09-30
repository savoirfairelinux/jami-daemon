#include "SFLPhonePlasmoid.h"
#include <QPainter>
#include <QFontMetrics>
#include <QSizeF>
#include <KIcon>
#include <KPushButton>

#include "../../src/lib/CallModel.h"
 
SFLPhonePlasmoid::SFLPhonePlasmoid(QObject* parent, const QVariantList& args)
    : Plasma::PopupApplet(parent, args), extenderItem(0), callInit(false), requierAttention(false)
{
   //m_svg.setImagePath("widgets/background");
   setBackgroundHints(DefaultBackground);
   
   CallModelConvenience::init();
   
   setMinimumSize(24,24);
}

void SFLPhonePlasmoid::dataUpdated(const QString& source, const Plasma::DataEngine::Data& data) 
{
   mainWidget->dataUpdated(source, data);

   //Prevent conferences from being initialized before calls
   if ((source == "calls") && (!callInit)) {
      sflphoneEngine->connectSource("conferences", this,0/*Update only if something happen*/);
      callInit = true;
   }
}
 
SFLPhonePlasmoid::~SFLPhonePlasmoid()
{

}
 
void SFLPhonePlasmoid::init()
{
   //contactEngine = dataEngine("akonadi");
   sflphoneEngine = dataEngine("sflphone");
   sflphoneEngine->connectSource("calls", this,0/*Update only if something happen*/);
   sflphoneEngine->connectSource("info", this,0/*Update only if something happen*/);
   sflphoneEngine->connectSource("history", this,0/*Update only if something happen*/);
   
   mainWidget = new MainWidget();
   connect(mainWidget,SIGNAL(requierAttention()),this,SLOT(notify()));

   //panelFrame = new Plasma::SvgWidget(&panel_icon);
   panelFrame = new Plasma::PushButton(this);

   QGraphicsLinearLayout* mainLayout = new QGraphicsLinearLayout(Qt::Vertical);

   //panel_icon.setImagePath("/usr/share/kde4/apps/sflphone-plasmoid/icons/sflphone.svg");
   panelFrame->setImage("/usr/share/kde4/apps/sflphone-plasmoid/icons/sflphone.svg");
   
   mainLayout->addItem(panelFrame);

   setLayout(mainLayout);

   //connect(panelFrame, SIGNAL(clicked(Qt::MouseButton)), this, SLOT(showCallPopup(Qt::MouseButton)));
   connect(panelFrame->nativeWidget(), SIGNAL(clicked()), this, SLOT(showCallPopup()));

   extender()->setEmptyExtenderMessage(i18n("no running jobs..."));
} 

void SFLPhonePlasmoid::constraintsEvent(Plasma::Constraints constraints)
{
   if (constraints& Plasma::FormFactorConstraint) {
      //FormFactor previous = formFaction;
      switch (formFactor()) {
         case Plasma::Planar:
         case Plasma::MediaCenter:
            formFaction = Desktop;
            m_orientation = Qt::Vertical;
            break;
         case Plasma::Horizontal:
            formFaction = Panel;
            m_orientation = Qt::Horizontal;
            break;
         case Plasma::Vertical:
            formFaction = Panel;
            m_orientation = Qt::Vertical;
            break;
      } 

          //if (previous != m_mode)
             //connectToEngine();
   } 
} 



//void SFLPhonePlasmoid::modeChanged(int index)
//{
   //switch (index) {
   //   case CALL:
   //      if (currentMode == HISTORY)
   //        switchPage(historyScrollArea,callScrollArea,contactScrollArea,true);
   //      else
   //         switchPage(contactScrollArea,callScrollArea,historyScrollArea,true);
   //      break;
   //   case HISTORY:
   //      if (currentMode == CALL)
   //         switchPage(callScrollArea,historyScrollArea,contactScrollArea,false);
   //      else
   //         switchPage(contactScrollArea,historyScrollArea,callScrollArea,true);
   //      break;
   //   case CONTACT:
   //      if (currentMode == CALL)
   //         switchPage(callScrollArea,contactScrollArea,historyScrollArea,false);
   //      else
   //         switchPage(historyScrollArea,contactScrollArea,callScrollArea,false);
   //      break;
   //}
   //currentMode = index;
//}

//void SFLPhonePlasmoid::switchPage(Plasma::ScrollWidget* oldFrame, Plasma::ScrollWidget* newFrame, Plasma::ScrollWidget* toHide, bool direction) 
//{
//   QPointF targetPos = oldFrame->pos();
//   newFrame->setPos(QPoint(targetPos.x() - newFrame->size().width() ,targetPos.y()));
//   Plasma::Animation *slideAnim = Plasma::Animator::create(Plasma::Animator::SlideAnimation);
//   slideAnim->setProperty("movementDirection", /*(direction)?*/Plasma::Animation::MoveRight/*:Plasma::Animation::MoveLeft*/);
//   slideAnim->setProperty("reference", Plasma::Animation::Center);
//   slideAnim->setProperty("distance", newFrame->size().width());
//   slideAnim->setProperty("duration",1000);
//   slideAnim->setTargetWidget(newFrame);
//
//   oldFrame->setPos(targetPos);
//   Plasma::Animation *slideAnimOut = Plasma::Animator::create(Plasma::Animator::SlideAnimation);
//   slideAnimOut->setProperty("movementDirection", /*(direction)?*/Plasma::Animation::MoveRight/*:Plasma::Animation::MoveLeft*/);
//   slideAnimOut->setProperty("reference", Plasma::Animation::Center);
//   slideAnimOut->setProperty("distance", newFrame->size().width());
//   slideAnimOut->setProperty("duration",1000);
//   slideAnimOut->setTargetWidget(oldFrame);

//   oldFrame->setVisible(true);
//   newFrame->setVisible(true);
//   toHide->setVisible(false);
   
//   slideAnim->start(QAbstractAnimation::DeleteWhenStopped);
//   slideAnimOut->start(QAbstractAnimation::DeleteWhenStopped);
//}

void SFLPhonePlasmoid::showCallPopup(/*Qt::MouseButton button*/) 
{
   //Q_UNUSED(button)
   if (!extenderItem) {
      extenderItem = new Plasma::ExtenderItem(extender());
      extenderItem->setTitle("SFL Phone");
      extenderItem->setWidget(mainWidget);
      //extenderItem->showCloseButton();
   }

   togglePopup();

   if (requierAttention) {
      //panel_icon.setImagePath("/usr/share/kde4/apps/sflphone-plasmoid/icons/sflphone.svg");
      panelFrame->setImage("/usr/share/kde4/apps/sflphone-plasmoid/icons/sflphone.svg");
      requierAttention = false;
   }
}

void SFLPhonePlasmoid::notify() 
{
   //panel_icon.setImagePath("/usr/share/kde4/apps/sflphone-plasmoid/icons/sflphone_notif.svg");
   panelFrame->setImage("/usr/share/kde4/apps/sflphone-plasmoid/icons/sflphone_notif.svg");
   requierAttention = true;
}

K_EXPORT_PLASMA_APPLET(sflphone, SFLPhonePlasmoid)

