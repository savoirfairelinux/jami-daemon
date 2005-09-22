#include "PhoneLineButton.hpp"

#include <QMouseEvent>

PhoneLineButton::PhoneLineButton(const QPixmap &released, 
				 const QPixmap &pressed,
				 unsigned int line,
				 QWidget *parent, 
				 Qt::WFlags flags)
  : JPushButton(released, pressed, parent, flags)
  , mLine(line)
{}

void 
PhoneLineButton::mouseReleaseEvent (QMouseEvent *e)
{
  switch (e->button()) {
  case Qt::LeftButton:
    release();
    // Emulate the left mouse click
    if (this->rect().contains(e->pos())) {
      emit clicked(mLine);
    }
    break;
    
  default:
    e->ignore();
    break;
  }
}
