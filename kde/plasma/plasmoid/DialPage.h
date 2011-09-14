#ifndef DIAL_PAGE
#define DIAL_PAGE

#include <Plasma/Frame>
#include <Plasma/PushButton>

class DialButton : public Plasma::PushButton 
{
   Q_OBJECT

   public:
      DialButton(QGraphicsWidget* parent) : Plasma::PushButton(parent) {
         connect(this, SIGNAL(clicked()), this, SLOT(clicked2()));
      }
      void setLetter(QString value) {
         letter = value;
      }
   private slots:
      void clicked2() {
         emit typed(letter);
      }
   signals:
      void typed(QString);
   private:
      QString letter;
};

class DialPage : public Plasma::Frame 
{
   Q_OBJECT
   public:
      DialPage();

   private slots:
      void charTyped(QString value);
      void call();
      void cancel();
      
   private:
      Plasma::Frame* currentNumber;
      QString currentNumber2;
      
   signals:
      void typed(QString);
      void call(QString);
      
};

#endif
