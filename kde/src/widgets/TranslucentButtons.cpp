#include "TranslucentButtons.h"

#include <QtGui/QPainter>
#include <KDebug>

#include <QtCore/QTimer>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDragLeaveEvent>
#include <QMimeData>

TranslucentButtons* TranslucentButtons::m_psActiveButton =0;

TranslucentButtons::TranslucentButtons(QWidget* parent):QPushButton(parent),m_enabled(true),m_pTimer(0),m_CurrentState(0),m_pImg(0)
{
   setAcceptDrops(true);
   m_CurrentColor = "black";
   m_CurrentColor.setAlpha(0);
}

TranslucentButtons::~TranslucentButtons()
{
   if (m_psActiveButton == this)
      m_psActiveButton=0;
}

void TranslucentButtons::paintEvent(QPaintEvent* event)
{
   Q_UNUSED(event)
   QPainter customPainter(this);
   //kDebug() << m_CurrentColor.name();
   //customPainter.setBackgroundMode( Qt::OpaqueMode );
   customPainter.setBackground(m_CurrentColor);
   customPainter.setBrush(m_CurrentColor);
   customPainter.setPen(Qt::NoPen);
   customPainter.drawRoundedRect(rect(), 10, 10);
   customPainter.setPen(m_Pen);

   if (m_pImg) {
      customPainter.drawImage(QRect(QPoint(rect().x()+rect().width()-50,10),QSize(40,rect().height()-20)),*m_pImg, QRectF(m_pImg->rect()));
   }

   QFont font = customPainter.font();
   font.setBold(true);
   customPainter.setFont(font);
   customPainter.drawText (rect(), Qt::AlignVCenter|Qt::AlignHCenter, text().replace("&","") );
}

void TranslucentButtons::setVisible(bool enabled)
{
   kDebug() << "Enabling!";
   if (m_enabled != enabled) {
      if (m_pTimer) {
         m_pTimer->stop();
         disconnect(m_pTimer);
      }
      m_pTimer = new QTimer(this); //TODO LEAK
      connect(m_pTimer, SIGNAL(timeout()), this, SLOT(changeVisibility()));
      m_step = 0;
      m_CurrentColor = "black";
      m_CurrentColor.setAlpha(0);
      repaint();
      m_pTimer->start(10);
      raise();
   }
   m_enabled = enabled;
   QWidget::setVisible(enabled);
}

void TranslucentButtons::changeVisibility()
{
   m_step++;
   m_CurrentColor.setAlpha(0.1*m_step*m_step);
   repaint();
   if (m_step >= 35)
      m_pTimer->stop();
}

void TranslucentButtons::dragEnterEvent ( QDragEnterEvent *e )
{
// //    if (m_psActiveButton && m_psActiveButton != this) {
// //       m_psActiveButton->dragLeaveEvent(0);
// //    }
//    m_psActiveButton = this;
//    kDebug() << "In button event";
//    int alpha = m_CurrentColor.alpha();
//    m_CurrentColor = "#FF0000";
//    m_CurrentColor.setAlpha(alpha);
//    repaint();
//    e->accept();
e->ignore();
}

void TranslucentButtons::dragMoveEvent  ( QDragMoveEvent  *e )
{
//    kDebug() << "In button move event";
//    int alpha = m_CurrentColor.alpha();
//    m_CurrentColor = "#FF0000";
//    m_CurrentColor.setAlpha(alpha);
//    e->accept();
e->ignore();
}

void TranslucentButtons::dragLeaveEvent ( QDragLeaveEvent *e )
{
//    kDebug() << "Button drag leave";
//    int alpha = m_CurrentColor.alpha();
//    m_CurrentColor = "black";
//    m_CurrentColor.setAlpha(alpha);
//    //m_CurrentColor = "black";
//    if (e)
//       e->ignore();
//    else {
//       repaint();
//    }
//    e->ignore();
e->ignore();
}

void TranslucentButtons::dropEvent(QDropEvent *e)
{
   kDebug() << "Drop accepted";
   emit dataDropped((QMimeData*)e->mimeData());
}

void TranslucentButtons::forceDragState(QDragEnterEvent *e)
{
   dragMoveEvent(e);
}

void TranslucentButtons::setHoverState(bool hover)
{
   if (hover != m_CurrentState) {
      if (hover) {
         int alpha = m_CurrentColor.alpha();
         m_CurrentColor = "grey";
         m_CurrentColor.setAlpha(alpha);
         m_Pen.setColor("black");
      }
      else {
         int alpha = m_CurrentColor.alpha();
         m_CurrentColor = "black";
         m_CurrentColor.setAlpha(alpha);
         m_Pen.setColor("white");
      }
      repaint();
      m_CurrentState = hover;
   }
}


void TranslucentButtons::setPixmap(QImage* img)
{
   m_pImg = img;
}

bool TranslucentButtons::event(QEvent* e)
{
//    switch (e->type()) {
//       case QEvent::None:
// kDebug() << "None"; break;
//       case QEvent::Timer:
// kDebug() << "Timer"; break;
//       case QEvent::MouseButtonPress:
// kDebug() << "MouseButtonPress"; break;
//       case QEvent::MouseButtonRelease:
// kDebug() << "MouseButtonRelease"; break;
//       case QEvent::MouseButtonDblClick:
// kDebug() << "MouseButtonDblClick"; break;
//       case QEvent::MouseMove:
// kDebug() << "MouseMove"; break;
//       case QEvent::KeyPress:
// kDebug() << "KeyPress"; break;
//       case QEvent::KeyRelease:
// kDebug() << "KeyRelease"; break;
//       case QEvent::FocusIn:
// kDebug() << "FocusIn"; break;
//       case QEvent::FocusOut:
// kDebug() << "FocusOut"; break;
//       case QEvent::Enter:
// kDebug() << "Enter"; break;
//       case QEvent::Leave:
// kDebug() << "Leave"; break;
//       case QEvent::Paint:
// kDebug() << "Paint"; break;
//       case QEvent::Move:
// kDebug() << "Move"; break;
//       case QEvent::Resize:
// kDebug() << "Resize"; break;
//       case QEvent::Create:
// kDebug() << "Create"; break;
//       case QEvent::Destroy:
// kDebug() << "Destroy"; break;
//       case QEvent::Show:
// kDebug() << "Show"; break;
//       case QEvent::Hide:
// kDebug() << "Hide"; break;
//       case QEvent::Close:
// kDebug() << "Close"; break;
//       case QEvent::Quit:
// kDebug() << "Quit"; break;
//       case QEvent::ParentChange:
// kDebug() << "ParentChange"; break;
//       case QEvent::ParentAboutToChange:
// kDebug() << "ParentAboutToChange"; break;
//       case QEvent::ThreadChange:
// kDebug() << "ThreadChange"; break;
//       case QEvent::WindowActivate:
// kDebug() << "WindowActivate"; break;
//       case QEvent::WindowDeactivate:
// kDebug() << "WindowDeactivate"; break;
//       case QEvent::ShowToParent:
// kDebug() << "ShowToParent"; break;
//       case QEvent::HideToParent:
// kDebug() << "HideToParent"; break;
//       case QEvent::Wheel:
// kDebug() << "Wheel"; break;
//       case QEvent::WindowTitleChange:
// kDebug() << "WindowTitleChange"; break;
//       case QEvent::WindowIconChange:
// kDebug() << "WindowIconChange"; break;
//       case QEvent::ApplicationWindowIconChange:
// kDebug() << "ApplicationWindowIconChange"; break;
//       case QEvent::ApplicationFontChange:
// kDebug() << "ApplicationFontChange"; break;
//       case QEvent::ApplicationLayoutDirectionChange:
// kDebug() << "ApplicationLayoutDirectionChange"; break;
//       case QEvent::ApplicationPaletteChange:
// kDebug() << "ApplicationPaletteChange"; break;
//       case QEvent::PaletteChange:
// kDebug() << "PaletteChange"; break;
//       case QEvent::Clipboard:
// kDebug() << "Clipboard"; break;
//       case QEvent::Speech:
// kDebug() << "Speech"; break;
//       case QEvent::MetaCall:
// kDebug() << "MetaCall"; break;
//       case QEvent::SockAct:
// kDebug() << "SockAct"; break;
//       case QEvent::WinEventAct:
// kDebug() << "WinEventAct"; break;
//       case QEvent::DeferredDelete:
// kDebug() << "DeferredDelete"; break;
//       case QEvent::DragEnter:
// kDebug() << "DragEnter"; break;
//       case QEvent::DragMove:
// kDebug() << "DragMove"; break;
//       case QEvent::DragLeave:
// kDebug() << "DragLeave"; break;
//       case QEvent::Drop:
// kDebug() << "Drop"; break;
//       case QEvent::DragResponse:
// kDebug() << "DragResponse"; break;
//       case QEvent::ChildAdded:
// kDebug() << "ChildAdded"; break;
//       case QEvent::ChildPolished:
// kDebug() << "ChildPolished"; break;
//       case QEvent::ChildRemoved:
// kDebug() << "ChildRemoved"; break;
//       case QEvent::ShowWindowRequest:
// kDebug() << "ShowWindowRequest"; break;
//       case QEvent::PolishRequest:
// kDebug() << "PolishRequest"; break;
//       case QEvent::Polish:
// kDebug() << "Polish"; break;
//       case QEvent::LayoutRequest:
// kDebug() << "LayoutRequest"; break;
//       case QEvent::UpdateRequest:
// kDebug() << "UpdateRequest"; break;
//       case QEvent::UpdateLater:
// kDebug() << "UpdateLater"; break;
//       case QEvent::EmbeddingControl:
// kDebug() << "EmbeddingControl"; break;
//       case QEvent::ActivateControl:
// kDebug() << "ActivateControl"; break;
//       case QEvent::DeactivateControl:
// kDebug() << "DeactivateControl"; break;
//       case QEvent::ContextMenu:
// kDebug() << "ContextMenu"; break;
//       case QEvent::InputMethod:
// kDebug() << "InputMethod"; break;
//       case QEvent::AccessibilityPrepare:
// kDebug() << "AccessibilityPrepare"; break;
//       case QEvent::TabletMove:
// kDebug() << "TabletMove"; break;
//       case QEvent::LocaleChange:
// kDebug() << "LocaleChange"; break;
//       case QEvent::LanguageChange:
// kDebug() << "LanguageChange"; break;
//       case QEvent::LayoutDirectionChange:
// kDebug() << "LayoutDirectionChange"; break;
//       case QEvent::Style:
// kDebug() << "Style"; break;
//       case QEvent::TabletPress:
// kDebug() << "TabletPress"; break;
//       case QEvent::TabletRelease:
// kDebug() << "TabletRelease"; break;
//       case QEvent::OkRequest:
// kDebug() << "OkRequest"; break;
//       case QEvent::HelpRequest:
// kDebug() << "HelpRequest"; break;
//       case QEvent::IconDrag:
// kDebug() << "IconDrag"; break;
//       case QEvent::FontChange:
// kDebug() << "FontChange"; break;
//       case QEvent::EnabledChange:
// kDebug() << "EnabledChange"; break;
//       case QEvent::ActivationChange:
// kDebug() << "ActivationChange"; break;
//       case QEvent::StyleChange:
// kDebug() << "StyleChange"; break;
//       case QEvent::IconTextChange:
// kDebug() << "IconTextChange"; break;
//       case QEvent::ModifiedChange:
// kDebug() << "ModifiedChange"; break;
//       case QEvent::MouseTrackingChange:
// kDebug() << "MouseTrackingChange"; break;
//       case QEvent::WindowBlocked:
// kDebug() << "WindowBlocked"; break;
//       case QEvent::WindowUnblocked:
// kDebug() << "WindowUnblocked"; break;
//       case QEvent::WindowStateChange:
// kDebug() << "WindowStateChange"; break;
//       case QEvent::ToolTip:
// kDebug() << "ToolTip"; break;
//       case QEvent::WhatsThis:
// kDebug() << "WhatsThis"; break;
//       case QEvent::StatusTip:
// kDebug() << "StatusTip"; break;
//       case QEvent::ActionChanged:
// kDebug() << "ActionChanged"; break;
//       case QEvent::ActionAdded:
// kDebug() << "ActionAdded"; break;
//       case QEvent::ActionRemoved:
// kDebug() << "ActionRemoved"; break;
//       case QEvent::FileOpen:
// kDebug() << "FileOpen"; break;
//       case QEvent::Shortcut:
// kDebug() << "Shortcut"; break;
//       case QEvent::ShortcutOverride:
// kDebug() << "ShortcutOverride"; break;
//       case QEvent::WhatsThisClicked:
// kDebug() << "WhatsThisClicked"; break;
//       case QEvent::QueryWhatsThis:
// kDebug() << "QueryWhatsThis"; break;
//       case QEvent::EnterWhatsThisMode:
// kDebug() << "EnterWhatsThisMode"; break;
//       case QEvent::LeaveWhatsThisMode:
// kDebug() << "LeaveWhatsThisMode"; break;
//       case QEvent::ZOrderChange:
// kDebug() << "ZOrderChange"; break;
//       case QEvent::HoverEnter:
// kDebug() << "HoverEnter"; break;
//       case QEvent::HoverLeave:
// kDebug() << "HoverLeave"; break;
//       case QEvent::HoverMove:
// kDebug() << "HoverMove"; break;
//       case QEvent::AccessibilityHelp:
// kDebug() << "AccessibilityHelp"; break;
//       case QEvent::AccessibilityDescription:
// kDebug() << "AccessibilityDescription"; break;
//       case QEvent::AcceptDropsChange:
// kDebug() << "AcceptDropsChange"; break;
//       case QEvent::MenubarUpdated:
// kDebug() << "MenubarUpdated"; break;
//       case QEvent::ZeroTimerEvent:
// kDebug() << "ZeroTimerEvent"; break;
//       case QEvent::GraphicsSceneMouseMove:
// kDebug() << "GraphicsSceneMouseMove"; break;
//       case QEvent::GraphicsSceneMousePress:
// kDebug() << "GraphicsSceneMousePress"; break;
//       case QEvent::GraphicsSceneMouseRelease:
// kDebug() << "GraphicsSceneMouseRelease"; break;
//       case QEvent::GraphicsSceneMouseDoubleClick:
// kDebug() << "GraphicsSceneMouseDoubleClick"; break;
//       case QEvent::GraphicsSceneContextMenu:
// kDebug() << "GraphicsSceneContextMenu"; break;
//       case QEvent::GraphicsSceneHoverEnter:
// kDebug() << "GraphicsSceneHoverEnter"; break;
//       case QEvent::GraphicsSceneHoverMove:
// kDebug() << "GraphicsSceneHoverMove"; break;
//       case QEvent::GraphicsSceneHoverLeave:
// kDebug() << "GraphicsSceneHoverLeave"; break;
//       case QEvent::GraphicsSceneHelp:
// kDebug() << "GraphicsSceneHelp"; break;
//       case QEvent::GraphicsSceneDragEnter:
// kDebug() << "GraphicsSceneDragEnter"; break;
//       case QEvent::GraphicsSceneDragMove:
// kDebug() << "GraphicsSceneDragMove"; break;
//       case QEvent::GraphicsSceneDragLeave:
// kDebug() << "GraphicsSceneDragLeave"; break;
//       case QEvent::GraphicsSceneDrop:
// kDebug() << "GraphicsSceneDrop"; break;
//       case QEvent::GraphicsSceneWheel:
// kDebug() << "GraphicsSceneWheel"; break;
//       case QEvent::KeyboardLayoutChange:
// kDebug() << "KeyboardLayoutChange"; break;
//       case QEvent::DynamicPropertyChange:
// kDebug() << "DynamicPropertyChange"; break;
//       case QEvent::TabletEnterProximity:
// kDebug() << "TabletEnterProximity"; break;
//       case QEvent::TabletLeaveProximity:
// kDebug() << "TabletLeaveProximity"; break;
//       case QEvent::NonClientAreaMouseMove:
// kDebug() << "NonClientAreaMouseMove"; break;
//       case QEvent::NonClientAreaMouseButtonPress:
// kDebug() << "NonClientAreaMouseButtonPress"; break;
//       case QEvent::NonClientAreaMouseButtonRelease:
// kDebug() << "NonClientAreaMouseButtonRelease"; break;
//       case QEvent::NonClientAreaMouseButtonDblClick:
// kDebug() << "NonClientAreaMouseButtonDblClick"; break;
//       case QEvent::MacSizeChange:
// kDebug() << "MacSizeChange"; break;
//       case QEvent::ContentsRectChange:
// kDebug() << "ContentsRectChange"; break;
//       case QEvent::MacGLWindowChange:
// kDebug() << "MacGLWindowChange"; break;
//       case QEvent::FutureCallOut:
// kDebug() << "FutureCallOut"; break;
//       case QEvent::GraphicsSceneResize:
// kDebug() << "GraphicsSceneResize"; break;
//       case QEvent::GraphicsSceneMove:
// kDebug() << "GraphicsSceneMove"; break;
//       case QEvent::CursorChange:
// kDebug() << "CursorChange"; break;
//       case QEvent::ToolTipChange:
// kDebug() << "ToolTipChange"; break;
//       case QEvent::NetworkReplyUpdated:
// kDebug() << "NetworkReplyUpdated"; break;
//       case QEvent::GrabMouse:
// kDebug() << "GrabMouse"; break;
//       case QEvent::UngrabMouse:
// kDebug() << "UngrabMouse"; break;
//       case QEvent::GrabKeyboard:
// kDebug() << "GrabKeyboard"; break;
//       case QEvent::UngrabKeyboard:
// kDebug() << "UngrabKeyboard"; break;
//       case QEvent::MacGLClearDrawable:
// kDebug() << "MacGLClearDrawable"; break;
//       case QEvent::StateMachineSignal:
// kDebug() << "StateMachineSignal"; break;
//       case QEvent::StateMachineWrapped:
// kDebug() << "StateMachineWrapped"; break;
//       case QEvent::TouchBegin:
// kDebug() << "TouchBegin"; break;
//       case QEvent::TouchUpdate:
// kDebug() << "TouchUpdate"; break;
//       case QEvent::TouchEnd:
// kDebug() << "TouchEnd"; break;
//       case QEvent::NativeGesture:
// kDebug() << "NativeGesture"; break;
//       case QEvent::RequestSoftwareInputPanel:
// kDebug() << "RequestSoftwareInputPanel"; break;
//       case QEvent::CloseSoftwareInputPanel:
// kDebug() << "CloseSoftwareInputPanel"; break;
//       case QEvent::UpdateSoftKeys:
// kDebug() << "UpdateSoftKeys"; break;
//       case QEvent::WinIdChange:
// kDebug() << "WinIdChange"; break;
//       case QEvent::Gesture:
// kDebug() << "Gesture"; break;
//       case QEvent::GestureOverride:
// kDebug() << "GestureOverride"; break;
//       case QEvent::User:
// kDebug() << "User"; break;
//       case QEvent::MaxUser:
//          kDebug() << "MaxUser"; break;
//    }
   return QPushButton::event(e);
}