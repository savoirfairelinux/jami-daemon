/****************************************************************************
** Form interface generated from reading ui file 'gui/qt/phonebook.ui'
**
** Created: Tue May 24 16:49:52 2005
**      by: The User Interface Compiler ($Id$)
**
** WARNING! All changes made in this file will be lost!
****************************************************************************/

#ifndef PHONEBOOK_H
#define PHONEBOOK_H

#include <qvariant.h>
#include <qdialog.h>

class QVBoxLayout;
class QHBoxLayout;
class QGridLayout;
class QSpacerItem;
class QPushButton;
class QListView;
class QListViewItem;

class PhoneBook : public QDialog
{
    Q_OBJECT

public:
    PhoneBook( QWidget* parent = 0, const char* name = 0, bool modal = FALSE, WFlags fl = 0 );
    ~PhoneBook();

    QPushButton* buttonHelp;
    QPushButton* buttonOk;
    QPushButton* buttonCancel;
    QListView* ContactList;
    QPushButton* buttonOk_2;
    QPushButton* buttonOk_3;
    QPushButton* buttonOk_4;

protected:
    QGridLayout* PhoneBookLayout;
    QHBoxLayout* Layout1;
    QSpacerItem* Horizontal_Spacing2;
    QVBoxLayout* layout10;
    QSpacerItem* spacer4;

protected slots:
    virtual void languageChange();

};

#endif // PHONEBOOK_H
