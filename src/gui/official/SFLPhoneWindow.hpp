#include <QObject>
#include <QMainWindow>
#include <list>

class PhoneLineButton;
class JPushButton;

class SFLPhoneWindow : public QMainWindow
{
  Q_OBJECT;

  friend class SFLPhoneApp;
  
public:
  SFLPhoneWindow();
  ~SFLPhoneWindow();

private:
  void initLineButtons();
  void initWindowButtons();

signals:
  void keyPressed(Qt::Key);

protected:
  void keyPressEvent(QKeyEvent *e);

private:
  std::list< PhoneLineButton * > mPhoneLineButtons;

  JPushButton *mCloseButton;
  JPushButton *mMinimizeButton;
};
