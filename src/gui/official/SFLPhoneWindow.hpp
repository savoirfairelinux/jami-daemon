#include <QMainWindow>
#include <QObject>
#include <QPoint>
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
  void initGUIButtons();
  void initLineButtons();
  void initWindowButtons();

signals:
  void keyPressed(Qt::Key);
  void reconnectAsked();
  void resendStatusAsked();

 public slots:
  void mousePressEvent(QMouseEvent *event);
  void mouseMoveEvent(QMouseEvent *event);

  /**
   * This function will prompt a message box, to ask
   * if the user want to reconnect to sflphoned.
   */
  void askReconnect();

  /**
   * This function will prompt a message box, to ask
   * if the user want to resend the getcallstatus request.
   */
  void askResendStatus();

protected:
  void keyPressEvent(QKeyEvent *e);

private:
  std::list< PhoneLineButton * > mPhoneLineButtons;

  JPushButton *mCloseButton;
  JPushButton *mMinimizeButton;

  JPushButton *mHangup;
  JPushButton *mHold;
  JPushButton *mOk;
  JPushButton *mClear;

  QPoint mLastPos;
};
