/****************************************************************************
** Form interface generated from reading ui file 'gui/qt/configurationpanel.ui'
**
** Created: Thu May 26 16:51:17 2005
**      by: The User Interface Compiler ($Id$)
**
** WARNING! All changes made in this file will be lost!
****************************************************************************/

#ifndef CONFIGURATIONPANEL_H
#define CONFIGURATIONPANEL_H

#include <qvariant.h>
#include <qpixmap.h>
#include <qdialog.h>

class QVBoxLayout;
class QHBoxLayout;
class QGridLayout;
class QSpacerItem;
class QListBox;
class QListBoxItem;
class QLabel;
class QFrame;
class QTabWidget;
class QWidget;
class QGroupBox;
class QLineEdit;
class QCheckBox;
class QPushButton;
class QButtonGroup;
class QRadioButton;
class QSpinBox;
class QComboBox;

class ConfigurationPanel : public QDialog
{
    Q_OBJECT

public:
    ConfigurationPanel( QWidget* parent = 0, const char* name = 0, bool modal = FALSE, WFlags fl = 0 );
    ~ConfigurationPanel();

    QListBox* Menu;
    QLabel* TitleTab;
    QFrame* line2;
    QTabWidget* Tab_Signalisations;
    QWidget* SIPPage;
    QGroupBox* groupBox1;
    QLabel* textLabel2;
    QLineEdit* fullName;
    QLineEdit* userPart;
    QLabel* textLabel3;
    QLabel* textLabel2_3;
    QLineEdit* username;
    QLineEdit* hostPart;
    QLineEdit* sipproxy;
    QLabel* textLabel3_2_2;
    QLineEdit* password;
    QLabel* textLabel1_3;
    QLabel* textLabel3_2;
    QCheckBox* autoregister;
    QPushButton* Register;
    QWidget* STUNPage;
    QGroupBox* groupBox3;
    QLabel* textLabel1_5;
    QLineEdit* STUNserver;
    QButtonGroup* stunButtonGroup;
    QRadioButton* useStunNo;
    QRadioButton* useStunYes;
    QWidget* DTMFPage;
    QGroupBox* SettingsDTMF;
    QCheckBox* playTones;
    QLabel* labelPulseLength;
    QSpinBox* pulseLength;
    QLabel* labelSendDTMF;
    QComboBox* sendDTMFas;
    QTabWidget* Tab_Audio;
    QWidget* DriversPage;
    QButtonGroup* DriverChoice;
    QRadioButton* ossButton;
    QRadioButton* alsaButton;
    QWidget* CodecsPage;
    QButtonGroup* CodecsChoice;
    QComboBox* codec1;
    QComboBox* codec2;
    QComboBox* codec3;
    QComboBox* codec4;
    QComboBox* codec5;
    QLabel* textLabel1_4;
    QLabel* textLabel1_4_2;
    QLabel* textLabel1_4_3;
    QLabel* textLabel1_4_4;
    QLabel* textLabel1_4_5;
    QWidget* RingPage;
    QComboBox* ringsChoice;
    QTabWidget* Tab_Video;
    QWidget* DriversPage_2;
    QWidget* CodecsPage_2;
    QTabWidget* Tab_Network;
    QWidget* DriversPage_3;
    QWidget* CodecsPage_3;
    QTabWidget* Tab_Preferences;
    QWidget* DriversPage_4;
    QComboBox* SkinChoice;
    QPushButton* buttonApplySkin;
    QWidget* TabPage;
    QLabel* textLabel1_2;
    QComboBox* zoneToneChoice;
    QCheckBox* confirmationToQuit;
    QCheckBox* checkedTray;
    QLabel* textLabel1_6;
    QLineEdit* voicemailNumber;
    QTabWidget* Tab_About;
    QWidget* DriversPage_5;
    QLabel* pixmapLabel1;
    QLabel* textLabel2_2;
    QWidget* CodecsPage_4;
    QLabel* textLabel1;
    QLabel* pixmapLabel2;
    QFrame* line1;
    QPushButton* buttonHelp;
    QPushButton* buttonSave;
    QPushButton* buttonOk;
    QPushButton* buttonCancel;

public slots:
    virtual void saveSlot();
    virtual void changeTabSlot();
    virtual void useStunSlot( int id );
    virtual void applySkinSlot();
    virtual void driverSlot( int id );

protected:
    QVBoxLayout* layout20;
    QHBoxLayout* layout31;
    QVBoxLayout* layout17;
    QVBoxLayout* layout24;
    QGridLayout* groupBox1Layout;
    QVBoxLayout* layout23;
    QSpacerItem* spacer9;
    QHBoxLayout* layout19;
    QSpacerItem* spacer7;
    QVBoxLayout* stunButtonGroupLayout;
    QGridLayout* SettingsDTMFLayout;
    QVBoxLayout* layout11;
    QHBoxLayout* layout10;
    QSpacerItem* spacer6;
    QHBoxLayout* layout7;
    QSpacerItem* spacer3;
    QHBoxLayout* layout8;
    QSpacerItem* spacer4;
    QVBoxLayout* DriverChoiceLayout;
    QGridLayout* CodecsChoiceLayout;
    QHBoxLayout* layout11_2;
    QVBoxLayout* layout9;
    QVBoxLayout* layout10_2;
    QVBoxLayout* layout17_2;
    QHBoxLayout* layout16;
    QSpacerItem* spacer5;
    QHBoxLayout* layout16_2;
    QSpacerItem* spacer6_2;
    QVBoxLayout* layout19_2;
    QHBoxLayout* layout28;
    QSpacerItem* Horizontal_Spacing2;

protected slots:
    virtual void languageChange();

private:
    QPixmap image0;
    QPixmap image1;

    void init();

};

#endif // CONFIGURATIONPANEL_H
