
#include <QMainWindow>
#include <list>

class JPushButton;

class SFLPhoneWindow : public QMainWindow
{
public:
  SFLPhoneWindow();
  ~SFLPhoneWindow();

  void initLineButtons();

private:
  std::list< JPushButton * > mLines;
};
