
#ifndef __PHONELINEBUTTON_HPP__
#define __PHONELINEBUTTON_HPP__

#include <QLabel>
#include <QObject>
#include <QPixmap>

#include "JPushButton.hpp"

class QTimer;


/**
 * This class Emulate a PushButton but takes two
 * images to display its state.
 */
class PhoneLineButton : public JPushButton
{
  Q_OBJECT
  
public:
  PhoneLineButton(const QString &released, 
		  const QString &pressed,
		  unsigned int line,
		  QWidget *parent, 
		  Qt::WFlags flags = 0);

  virtual ~PhoneLineButton(){}
  
signals:
  void clicked(unsigned int);
  
public slots:
  virtual void suspend();
  virtual void press();
  virtual void release();
  
private slots:
  void swap();
  
protected:
  void mouseReleaseEvent(QMouseEvent *);

private:
  unsigned int mLine;
  QTimer *mTimer;
  unsigned int mFace;
  
};

#endif	// defined(__J_PUSH_BUTTON_H__)
