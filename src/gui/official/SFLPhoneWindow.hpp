
#include <QMainWindow>
#include <list>

class JPushButton;

class SFLPhoneWindow : public QMainWindow
{
public:
  SFLPhoneWindow();
  ~SFLPhoneWindow();

private:
  void initLineButtons();

private:
  std::list< JPushButton * > mLineButtons;
  std::list< PhoneLine > mPhoneLines;
};
