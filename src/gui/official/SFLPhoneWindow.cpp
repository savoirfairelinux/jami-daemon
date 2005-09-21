#include "SFLPhoneWindow.hpp"

#include <QLabel>
#include <QPixmap>
#include <iostream>

#include "JPushButton.hpp"

#define NBLINES 6


SFLPhoneWindow::SFLPhoneWindow()
  : QMainWindow(NULL, 0)
  //	Qt::FramelessWindowHint)
{
  QLabel *l = new QLabel(this);
  QPixmap main(":/images/main-img.png");
  l->setPixmap(main);
  resize(main.size());
  l->resize(main.size());

  initLineButtons();
}

SFLPhoneWindow::~SFLPhoneWindow()
{
  int i = 0;
  i++;
}

void 
SFLPhoneWindow::initLineButtons()
{
  int xpos = 21;
  int ypos = 151;
  int offset = 31;
  for(int i = 0; i < NBLINES; i++) {
    std::cout << i << std::endl;
    JPushButton *line = new JPushButton(QPixmap(QString(":/images/line") + 
						QString::number(i + 1) + 
						"off-img.png"),
					QPixmap(QString(":/images/line") + 
						QString::number(i + 1) + 
						"on-img.png"),
					this);
    line->move(xpos, ypos);
    xpos += offset;
    mLines.push_back(line);
  }
}

