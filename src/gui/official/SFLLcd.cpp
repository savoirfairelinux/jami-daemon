#include <QDateTime>
#include <QPainter>
#include <QPaintEvent>

#include "globals.h"
#include "JPushButton.hpp"
#include "SFLLcd.hpp"

#define FONT_FAMILY	"Courier"
// Others fixed font support "Monospace", "Fixed", "MiscFixed"
#define FONT_SIZE	10

SFLLcd::SFLLcd(QWidget *parent, Qt::WFlags flags)
  : QLabel(parent, flags)
  , mScreen(":/sflphone/images/screen_main")
  , mOverscreen(JPushButton::transparize(":/sflphone/images/overscreen.png"))
  , mGlobalStatusPos(-1)
  , mLineStatusPos(-1)
  , mIsTimed(false)
  , mFont(FONT_FAMILY, FONT_SIZE)
{
  mFont.setBold(true);

  setPixmap(mScreen);
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
SFLLcd::setLineStatus(const QString &line)
{
  if(textIsTooBig(line)) {
    mGlobalStatusPos = 0;
  }
  else {
    mLineStatusPos = -1;
  }
  mLineStatus = line;
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

  p.drawText(QPoint(margin, mScreen.size().height() - margin), getTimeStatus());
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

  if(pos >= tmp.size() + nbCharBetween) {
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
      tmp.chop(1);
    }
  }

  return tmp;
}

