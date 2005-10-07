#include <qdatetime.h>
#include <qpainter.h>
#include <qevent.h>

#include "globals.h"
#include "JPushButton.hpp"
#include "SFLLcd.hpp"

#define FONT_FAMILY	"Courier"
// Others fixed font support "Monospace", "Fixed", "MiscFixed"
#define FONT_SIZE	10

#define SCREEN "screen_main"
#define OVERSCREEN "overscreen"
SFLLcd::SFLLcd(QWidget *parent)
  : QLabel(parent)
  , mScreen(JPushButton::transparize(QPixmap::fromMimeSource(SCREEN)))
  , mOverscreen(JPushButton::transparize(QPixmap::fromMimeSource(OVERSCREEN)))
  , mGlobalStatusPos(-1)
  , mLineStatusPos(-1)
  , mBufferStatusPos(-1)
  , mActionPos(-1)
  , mIsTimed(false)
  , mFont(FONT_FAMILY, FONT_SIZE)
{
  resize(mScreen.size());
  move(22,44);
  
  
  mTimer = new QTimer(this);
  QObject::connect(mTimer, SIGNAL(timeout()), 
		   this, SLOT(updateText()));
  QObject::connect(mTimer, SIGNAL(timeout()), 
		   this, SLOT(update()));
  mTimer->start(100);
}

void
SFLLcd::updateText()
{
  if(mGlobalStatusPos >= 0) {
    mGlobalStatusPos++;
  }

  if(mLineStatusPos >= 0) {
    mLineStatusPos++;
  }

  if(mBufferStatusPos >= 0) {
    mBufferStatusPos++;
  }

  if(mActionPos >= 0) {
    mActionPos++;
  }
}

void
SFLLcd::startTiming()
{
    mIsTimed = true;
    mTime.start();
}

void
SFLLcd::stopTiming()
{
  mIsTimed = false;
}

void
SFLLcd::setGlobalStatus(const QString &global)
{
  if(textIsTooBig(global)) {
    mGlobalStatusPos = 0;
  }
  else {
    mGlobalStatusPos = -1;
  }
  mGlobalStatus = global;
}

void
SFLLcd::setBufferStatus(const QString &buffer)
{
  if(textIsTooBig(buffer)) {
    mBufferStatusPos = 0;
  }
  else {
    mBufferStatusPos = -1;
  }
  mBufferStatus = buffer;
}

void
SFLLcd::setLineStatus(const QString &line)
{
  if(textIsTooBig(line)) {
    mLineStatusPos = 0;
  }
  else {
    mLineStatusPos = -1;
  }
  mLineStatus = line;
}

void
SFLLcd::setAction(const QString &line)
{
  if(textIsTooBig(line)) {
    mActionPos = 0;
  }
  else {
    mActionPos = -1;
  }
  mAction = line;
}

QString
SFLLcd::getTimeStatus()
{
  if(mIsTimed) {
    int seconds = mTime.elapsed() / 1000 ;
    return QTime(seconds / 60 / 60, seconds / 60, seconds % 60).toString("hh:mm:ss");
  }
  else {
    QTime t(QTime::currentTime());
    QString s;
    if(t.second() % 2) {
      s = t.toString("hh:mm");
    }
    else {
      s = t.toString("hh mm");
    }

    return s;
  }
}

void
SFLLcd::paintEvent(QPaintEvent *) 
{
  // Painter settings 
  QFontMetrics fm(mFont);

  int margin = 2;

  QPainter p(this);
  p.setFont(mFont);
  p.drawPixmap(0,0, mScreen);
  p.drawText(QPoint(margin, fm.height()), 
	     extractVisibleText(mGlobalStatus, mGlobalStatusPos));
  p.drawText(QPoint(margin, 2*fm.height()), 
	     extractVisibleText(mLineStatus, mLineStatusPos));
  p.drawText(QPoint(margin, 3*fm.height()), 
	     extractVisibleText(mAction, mActionPos));
  p.drawText(QPoint(margin, 4*fm.height()), 
	     extractVisibleText(mBufferStatus, mBufferStatusPos));

  p.drawText(QPoint(margin, mScreen.size().height() - margin), getTimeStatus());
  //p.drawPixmap(0,0, mScreen);
  p.drawPixmap(0,0, mOverscreen);
  p.end();
}

bool 
SFLLcd::textIsTooBig(const QString &text) 
{
  QFontMetrics fm(mFont);

  int screenWidth = mScreen.width() - 4;
  int textWidth = fm.boundingRect(text).width();

  if(textWidth > screenWidth) {
    return true;
  }
  else {
    return false;
  }
}

QString
SFLLcd::extractVisibleText(const QString &text, int &pos) 
{
  QFontMetrics fm(mFont);
  QString tmp(text);

  int nbCharBetween = 8;

  if(pos >= tmp.length() + nbCharBetween) {
    pos = 0;
  }

  // Chop the text until it's not too big
  if(textIsTooBig(tmp)) {
    // We add automatiquely the space the the text again at 
    // the end.
    tmp += QString().fill(QChar(' '), nbCharBetween);
    tmp += text;
    
    if(pos == -1) {
      pos = 0;
    }

    tmp.remove(0, pos);
    while(textIsTooBig(tmp)) {
      tmp.remove(tmp.length() - 1, 1);
    }
  }

  return tmp;
}

