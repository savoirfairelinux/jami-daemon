
#include <QMainWindow>
#include <list>

class PhoneLineButton;

class SFLPhoneWindow : public QMainWindow
{
  friend class SFLPhoneApp;

public:
  SFLPhoneWindow();
  ~SFLPhoneWindow();

private:
  void initLineButtons();

private:
  std::list< PhoneLineButton * > mPhoneLineButtons;
};
