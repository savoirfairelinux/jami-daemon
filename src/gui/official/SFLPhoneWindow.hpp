#include <qlabel.h>
#include <qmainwindow.h>
#include <qobject.h>
#include <qpoint.h>
#include <list>

#include "ConfigurationPanel.h"

class JPushButton;
class PhoneLineButton;
class SFLLcd;
class VolumeControl;

class SFLPhoneWindow : public QMainWindow
{
  Q_OBJECT;

  friend class SFLPhoneApp;
  
public:
  SFLPhoneWindow();
  ~SFLPhoneWindow();

private:
  void initLCD();
  void initGUIButtons();
  void initLineButtons();
  void initWindowButtons();

signals:
  void keyPressed(Qt::Key);
  void reconnectAsked();
  void resendStatusAsked();
  void volumeUpdated(int);
  void micVolumeUpdated(int);

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
  void askResendStatus(QString);

  void showSetup();

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
  JPushButton *mMute;
  JPushButton *mSetup;
  
  VolumeControl *mVolume;
  VolumeControl *mMicVolume;

  SFLLcd *mLcd;
  QLabel *mMain;

  QPoint mLastPos;

  ConfigurationPanel *mSetupPanel;
};
