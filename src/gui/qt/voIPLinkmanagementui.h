/****************************************************************************
** Form interface generated from reading ui file 'gui/qt/voIPLinkmanagement.ui'
**
** Created: Thu May 26 16:51:18 2005
**      by: The User Interface Compiler ($Id$)
**
** WARNING! All changes made in this file will be lost!
****************************************************************************/

#ifndef VOIPLINKMANAGEMENT_H
#define VOIPLINKMANAGEMENT_H

#include <qvariant.h>
#include <qpixmap.h>
#include <qwidget.h>

class QVBoxLayout;
class QHBoxLayout;
class QGridLayout;
class QSpacerItem;
class QListBox;
class QListBoxItem;
class QPushButton;
class QGroupBox;
class QLabel;
class QLineEdit;
class QCheckBox;

class VoIPLinkManagement : public QWidget
{
    Q_OBJECT

public:
    VoIPLinkManagement( QWidget* parent = 0, const char* name = 0, WFlags fl = 0 );
    ~VoIPLinkManagement();

    QListBox* listVoiplink;
    QPushButton* buttonAddVoiplink;
    QPushButton* buttonRemoveVoiplink;
    QPushButton* up;
    QPushButton* down;
    QGroupBox* parameters;
    QLabel* textLabel1;
    QLabel* textLabel2;
    QLabel* textLabel3;
    QLabel* textLabel4;
    QLabel* textLabel5;
    QLabel* textLabel6;
    QLineEdit* fullName;
    QLineEdit* userPart;
    QLineEdit* hostPart;
    QLineEdit* authUser;
    QLineEdit* password;
    QLineEdit* proxy;
    QCheckBox* autoRegister;

public slots:
    virtual void changeParamSlot();
    virtual void moveUpItemSlot();
    virtual void moveDownItemSlot();
    virtual void addVoIPLinkSlot();
    virtual void removeVoIPLinkSlot();

protected:
    QGridLayout* layout15;
    QVBoxLayout* layout30;
    QVBoxLayout* layout14;
    QGridLayout* layout26;
    QVBoxLayout* layout24;
    QVBoxLayout* layout23;

protected slots:
    virtual void languageChange();

private:
    QPixmap image0;
    QPixmap image1;

    void init();

};

#endif // VOIPLINKMANAGEMENT_H
