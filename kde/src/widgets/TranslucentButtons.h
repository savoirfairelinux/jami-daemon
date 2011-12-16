#ifndef TRANSLUCENTBUTTONS_H
#define TRANSLUCENTBUTTONS_H
#include <QtGui/QPushButton>
#include <QtGui/QPen>

class QTimer;
class QMimeData;
class QImage;

///@class TranslucentButtons Fancy buttons for the call widget
class TranslucentButtons : public QPushButton
{
   Q_OBJECT
public:
   TranslucentButtons(QWidget* parent);
   ~TranslucentButtons();
   void forceDragState(QDragEnterEvent *e);
   void setHoverState(bool hover);
   void setPixmap(QImage* img);
protected:
   virtual void paintEvent(QPaintEvent* event);
   virtual void dragEnterEvent ( QDragEnterEvent *e );
   virtual void dragMoveEvent  ( QDragMoveEvent  *e );
   virtual void dragLeaveEvent ( QDragLeaveEvent *e );
   virtual void dropEvent      ( QDropEvent      *e );
   virtual bool event(QEvent* e);
private:
   bool m_enabled;
   uint m_step;
   QTimer* m_pTimer;
   QColor m_CurrentColor;
   QPen   m_Pen;
   bool m_CurrentState;
   QImage* m_pImg;
   static TranslucentButtons* m_psActiveButton; /*Workaround for a Qt bug*/
public slots:
   void setVisible(bool enabled);
private slots:
   void changeVisibility();
signals:
   void dataDropped(QMimeData*);
};
#endif