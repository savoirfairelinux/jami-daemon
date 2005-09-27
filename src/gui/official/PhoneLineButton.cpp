#include "globals.h" 

#include "PhoneLineButton.hpp"

#include <QMouseEvent>
#include <QTimer>


PhoneLineButton::PhoneLineButton(const QPixmap &released, 
				 const QPixmap &pressed,
				 unsigned int line,
				 QWidget *parent, 
				 Qt::WFlags flags)
  : JPushButton(released, pressed, parent, flags)
  , mLine(line)
  , mFace(0)
{
  mTimer = new QTimer(this);
  connect(mTimer, SIGNAL(timeout()),
	  this, SLOT(swap()));
}

void
PhoneLineButton::suspend()
{
  _debug("Swapping started.\n");
  if(isPressed()) {
    mFace = 1;
  }
  else {
    mFace = 0;
  }
  swap();
  mTimer->start(500);
}

void
PhoneLineButton::swap()
{
  mFace = (mFace + 1) % 2;
  resize(mImages[mFace].size());
  setPixmap(mImages[mFace]);
}

void 
PhoneLineButton::press()
{
  _debug("Pressed");
  mTimer->stop();
  JPushButton::press();
}

void 
PhoneLineButton::release()
{
  mTimer->stop();
  JPushButton::release();
}

void 
PhoneLineButton::mouseReleaseEvent (QMouseEvent *e)
{
  switch (e->button()) {
  case Qt::LeftButton:
    // Emulate the left mouse click
    if (this->rect().contains(e->pos())) {
      emit clicked(mLine);
    }
    else {
      release();
    }
    break;
    
  default:
    e->ignore();
    break;
  }
}
