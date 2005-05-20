/****************************************************************************
** Form interface generated from reading ui file 'gui/qt/url_input.ui'
**
** Created: Fri May 20 14:27:25 2005
**      by: The User Interface Compiler ($Id$)
**
** WARNING! All changes made in this file will be lost!
****************************************************************************/

#ifndef URL_INPUT_H
#define URL_INPUT_H

#include <qvariant.h>
#include <qdialog.h>

class QVBoxLayout;
class QHBoxLayout;
class QGridLayout;
class QSpacerItem;
class QPushButton;
class QLineEdit;

class URL_Input : public QDialog
{
    Q_OBJECT

public:
    URL_Input( QWidget* parent = 0, const char* name = 0, bool modal = FALSE, WFlags fl = 0 );
    ~URL_Input();

    QPushButton* buttonOK;
    QPushButton* buttonCancel;
    QLineEdit* url;

protected:

protected slots:
    virtual void languageChange();

};

#endif // URL_INPUT_H
