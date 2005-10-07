#include "globals.h" 

#include "PhoneLineButton.hpp"

#include <qevent.h>
#include <qtimer.h>


PhoneLineButton::PhoneLineButton(const QString &released, 
				 const QString &pressed,
				 unsigned int line,
				 QWidget *parent)
  : JPushButton(released, pressed, parent)
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
