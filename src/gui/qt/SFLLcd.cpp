/*
 *  Copyright (C) 2004-2005 Savoir-Faire Linux inc.
 *  Author: Yan Morin <yan.morin@savoirfairelinux.com>
 *  Author: Jean-Philippe Barrette-LaPierre
 *             <jean-philippe.barrette-lapierre@savoirfairelinux.com>
 *                                                                              
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *                                                                              
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *                                                                              
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <qdatetime.h>
#include <qpainter.h>
#include <qevent.h>
#include <qdragobject.h>

#include "globals.h"
#include "JPushButton.hpp"
#include "SFLLcd.hpp"
#include "TransparentWidget.hpp"
// should send a signal... not include this
#include "PhoneLineManager.hpp"

#include "DebugOutput.hpp" // to remove after testing


#define FONT_FAMILY	"Courier"
// Others fixed font support "Monospace", "Fixed", "MiscFixed"
#define FONT_SIZE	10

#define SCREEN "screen_main.png"
#define OVERSCREEN "overscreen.png"

SFLLcd::SFLLcd(QWidget *parent)
  : QLabel(parent, "SFLLcd", Qt::WNoAutoErase)
  , mScreen(TransparentWidget::retreive(SCREEN))
  , mOverscreen(TransparentWidget::retreive(OVERSCREEN))
  , mGlobalStatusPos(-1)
  , mUnselectedLineStatusPos(-1)
  , mLineStatusPos(-1)
  , mBufferStatusPos(-1)
  , mActionPos(-1)
  , mIsTimed(false)
  , mFont(FONT_FAMILY, FONT_SIZE)
{
  resize(mScreen.size());
  move(22,44);
  
  mUnselectedLineTimer = new QTimer(this);
  QObject::connect(mUnselectedLineTimer, SIGNAL(timeout()), 
		   this, SLOT(updateGlobalText()));

  mTimer = new QTimer(this);
  QObject::connect(mTimer, SIGNAL(timeout()), 
		   this, SLOT(updateText()));
  QObject::connect(mTimer, SIGNAL(timeout()), 
		   this, SLOT(update()));
  mTimer->start(100);

  setAcceptDrops(TRUE);
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
SFLLcd::updateGlobalText()
{
  mUnselectedLineStatus = "";
}

void
SFLLcd::setLineTimer(QTime time)
{
    mIsTimed = true;
    mTime = time;
}

void
SFLLcd::clearLineTimer()
{
  mIsTimed = false;
}

void
SFLLcd::setGlobalStatus(QString global)
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
SFLLcd::setBufferStatus(QString buffer)
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
SFLLcd::setLineStatus(QString line)
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
SFLLcd::setUnselectedLineStatus(QString line)
{
  if(textIsTooBig(line)) {
    mUnselectedLineStatusPos = 0;
  }
  else {
    mUnselectedLineStatusPos = -1;
  }
  mUnselectedLineStatus = line;
  mUnselectedLineTimer->start(3000, true);
}

void
SFLLcd::setAction(QString line)
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
SFLLcd::paintEvent(QPaintEvent *event) 
{
  static QPixmap pixmap(size());

  QRect rect = event->rect();
  QSize newSize = rect.size().expandedTo(pixmap.size());
  pixmap.resize(newSize);
  pixmap.fill(this, rect.topLeft());
  QPainter p(&pixmap, this);

  // Painter settings 
  QFontMetrics fm(mFont);

  int *globalStatusPos;
  QString globalStatus;
  if(mUnselectedLineStatus.length() > 0) { 
    globalStatus = mUnselectedLineStatus;
    globalStatusPos = &mUnselectedLineStatusPos;
  }
  else {
    globalStatus = mGlobalStatus;
    globalStatusPos = &mGlobalStatusPos;
  }

  int margin = 2;
  p.setFont(mFont);
  p.drawPixmap(0,0, mScreen);
  p.drawText(QPoint(margin, fm.height()), 
	     extractVisibleText(globalStatus, *globalStatusPos));
  p.drawText(QPoint(margin, 2*fm.height()), 
	     extractVisibleText(mLineStatus, mLineStatusPos));
  p.drawText(QPoint(margin, 3*fm.height()), 
	     extractVisibleText(mAction, mActionPos));
  p.drawText(QPoint(margin, 4*fm.height()), 
	     extractVisibleText(mBufferStatus, mBufferStatusPos));

  p.drawText(QPoint(margin, mScreen.size().height() - margin), getTimeStatus());
  p.drawPixmap(0,0, mOverscreen);
  p.end();

  bitBlt(this, event->rect().topLeft(), &pixmap);
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

  if(pos > 0 && ((unsigned int)pos >= tmp.length() + nbCharBetween)) {
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

/**
 * Drag and drop handler : accept text drag
 */
void 
SFLLcd::dragEnterEvent(QDragEnterEvent* event)
{
  event->accept(
    QTextDrag::canDecode(event)
  );
}

/**
 * Drag and drop handler : make a call with text
 */
void 
SFLLcd::dropEvent(QDropEvent* event)
{
  QString text;

  if ( QTextDrag::decode(event, text) && !text.isEmpty() ) {
    PhoneLineManager::instance().makeNewCall(text);
  }
}

void
SFLLcd::mousePressEvent( QMouseEvent *e)
{
  if (e && e->button() == Qt::MidButton) {
    emit midClicked();
  }
}
