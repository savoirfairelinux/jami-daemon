/****************************************************************************
** Form interface generated from reading ui file 'url_input.ui'
**
** Created: Sat Dec 25 20:42:50 2004
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

    QPushButton* buttonCancel;
    QLineEdit* url;
    QPushButton* buttonOK;

protected:

protected slots:
    virtual void languageChange();

};

#endif // URL_INPUT_H
