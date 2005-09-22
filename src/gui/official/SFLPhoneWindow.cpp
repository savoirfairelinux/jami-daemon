#include "SFLPhoneWindow.hpp"

#include <QLabel>
#include <QPixmap>
#include <iostream>

#include "JPushButton.hpp"

#define NBLINES 6


SFLPhoneWindow::SFLPhoneWindow()
  : QMainWindow(NULL, 0)
  , mSession()
  , mAccount(mSession.getAccount("monaccount")
{
  // Initialize the background image
  QLabel *l = new QLabel(this);
  QPixmap main(":/images/main-img.png");
  l->setPixmap(main);
  resize(main.size());
  l->resize(main.size());

//   QLabel *os = new QLabel(this);
//   QPixmap overscreen(":/images/overscreen.png");
//   os->setPixmap(overscreen);
//   os->resize(overscreen.size());
//   os->move(22,44);
  

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
    connect(line, clicked()
	    &mPhoneLineManager, clicked());
    line->move(xpos, ypos);
    xpos += offset;
    mLineButtons.push_back(line);
  }
}

