#include "DialPage.h"

#include <Plasma/PushButton>
#include <Plasma/FrameSvg>
#include <QGraphicsGridLayout>
#include <QLabel>
#include <KPushButton>
#include <QHBoxLayout>
#include <QDebug>

DialPage::DialPage()
{
   QGraphicsGridLayout* mainLayout = new QGraphicsGridLayout();
   setLayout(mainLayout);
   currentNumber = new Plasma::Frame(this);
   currentNumber->setText("Dial");
   currentNumber->setFrameShadow(Plasma::Frame::Sunken);
   currentNumber->setMinimumSize(0,50);
   currentNumber->setStyleSheet("background-color:#AAAAFF;border-size:2px;border-style:sunken;");
   //currentNumber->setEnabledBorders(FrameSvg::EnabledBorders::Raised);
   mainLayout->addItem(currentNumber,0,0,1,6);

   QString numbers[12] =
       {"1", "2", "3",
        "4", "5", "6",
        "7", "8", "9",
        "*", "0", "#"};

   QString texts[12] =
       {  ""  ,  "abc",  "def" ,
        "ghi" ,  "jkl",  "mno" ,
        "pqrs",  "tuv",  "wxyz",
          ""  ,   ""  ,   ""   };

   for(int i = 0 ; i < 12 ; i++) {
      DialButton* newButton = new DialButton(this);
      newButton->setMinimumHeight(40);
      newButton->setLetter(numbers[i]);
      newButton->setText(numbers[i]+((!texts[i].isEmpty())?("\n"+texts[i]):""));
      mainLayout->addItem(newButton,1+i/3,2*(i%3),1,2);
      connect(newButton,SIGNAL(typed(QString)),this, SLOT(charTyped(QString)));
   }

   Plasma::PushButton* newButton = new Plasma::PushButton(this);
   newButton->setText("Call");
   newButton->setIcon(KIcon("/usr/share/kde4/apps/sflphone-plasmoid/icons/outgoing.svg"));
   mainLayout->addItem(newButton,5,0,1,3);

   Plasma::PushButton* cancelButton = new Plasma::PushButton(this);
   cancelButton->setText("Cancel");
   cancelButton->setIcon(KIcon("/usr/share/kde4/apps/sflphone-plasmoid/icons/hang_up.svg"));
   mainLayout->addItem(cancelButton,5,3,1,3);

   connect(newButton, SIGNAL(clicked()), this, SLOT(call()));
   connect(cancelButton, SIGNAL(clicked()), this, SLOT(cancel()));
}

void DialPage::charTyped(QString value)
{
   currentNumber2 += value;
   currentNumber->setText(currentNumber2);
}

void DialPage::call()
{
   emit call(currentNumber2);
   currentNumber2 = "";
   currentNumber->setText("Dial");
}

void DialPage::cancel()
{
   currentNumber2 = "";
   currentNumber->setText("Dial");
}
