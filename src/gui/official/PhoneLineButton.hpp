
#ifndef __PHONELINEBUTTON_HPP__
#define __PHONELINEBUTTON_HPP__

#include <QLabel>
#include <QObject>
#include <QPixmap>

#include "JPushButton.hpp"

/**
 * This class Emulate a PushButton but takes two
 * images to display its state.
 */
class PhoneLineButton : public JPushButton
{
  Q_OBJECT
  
public:
  PhoneLineButton(const QPixmap &released, 
		  const QPixmap &pressed,
		  unsigned int line,
		  QWidget *parent, 
		  Qt::WFlags flags = 0);

  virtual ~PhoneLineButton(){}
  
protected:
  void mouseReleaseEvent(QMouseEvent *);
  
signals:
  void clicked(unsigned int);

private:
  unsigned int mLine;

};

#endif	// defined(__J_PUSH_BUTTON_H__)
