/****************************************************************************
** Form implementation generated from reading ui file 'gui/qt/phonebook.ui'
**
** Created: Fri May 20 14:27:25 2005
**      by: The User Interface Compiler ($Id$)
**
** WARNING! All changes made in this file will be lost!
****************************************************************************/

#include "gui/qt/phonebookui.h"

#include <qvariant.h>
#include <qpushbutton.h>
#include <qheader.h>
#include <qlistview.h>
#include <qlayout.h>
#include <qtooltip.h>
#include <qwhatsthis.h>

/*
 *  Constructs a PhoneBook as a child of 'parent', with the
 *  name 'name' and widget flags set to 'f'.
 *
 *  The dialog will by default be modeless, unless you set 'modal' to
 *  TRUE to construct a modal dialog.
 */
PhoneBook::PhoneBook( QWidget* parent, const char* name, bool modal, WFlags fl )
    : QDialog( parent, name, modal, fl )
{
    if ( !name )
	setName( "PhoneBook" );
    setSizeGripEnabled( TRUE );
    PhoneBookLayout = new QGridLayout( this, 1, 1, 11, 6, "PhoneBookLayout"); 

    Layout1 = new QHBoxLayout( 0, 0, 6, "Layout1"); 

    buttonHelp = new QPushButton( this, "buttonHelp" );
    buttonHelp->setAutoDefault( TRUE );
    Layout1->addWidget( buttonHelp );
    Horizontal_Spacing2 = new QSpacerItem( 20, 20, QSizePolicy::Expanding, QSizePolicy::Minimum );
    Layout1->addItem( Horizontal_Spacing2 );

    buttonOk = new QPushButton( this, "buttonOk" );
    buttonOk->setAutoDefault( TRUE );
    buttonOk->setDefault( TRUE );
    Layout1->addWidget( buttonOk );

    buttonCancel = new QPushButton( this, "buttonCancel" );
    buttonCancel->setAutoDefault( TRUE );
    Layout1->addWidget( buttonCancel );

    PhoneBookLayout->addMultiCellLayout( Layout1, 1, 1, 0, 1 );

    ContactList = new QListView( this, "ContactList" );
    ContactList->addColumn( tr( "Name" ) );
    ContactList->addColumn( tr( "Address SIP" ) );
    ContactList->addColumn( tr( "Mail" ) );
    ContactList->addColumn( tr( "Phone" ) );

    PhoneBookLayout->addWidget( ContactList, 0, 0 );

    layout10 = new QVBoxLayout( 0, 0, 6, "layout10"); 

    buttonOk_2 = new QPushButton( this, "buttonOk_2" );
    buttonOk_2->setAutoDefault( TRUE );
    buttonOk_2->setDefault( TRUE );
    layout10->addWidget( buttonOk_2 );

    buttonOk_3 = new QPushButton( this, "buttonOk_3" );
    buttonOk_3->setAutoDefault( TRUE );
    buttonOk_3->setDefault( TRUE );
    layout10->addWidget( buttonOk_3 );

    buttonOk_4 = new QPushButton( this, "buttonOk_4" );
    buttonOk_4->setAutoDefault( TRUE );
    buttonOk_4->setDefault( TRUE );
    layout10->addWidget( buttonOk_4 );
    spacer4 = new QSpacerItem( 21, 91, QSizePolicy::Minimum, QSizePolicy::Expanding );
    layout10->addItem( spacer4 );

    PhoneBookLayout->addLayout( layout10, 0, 1 );
    languageChange();
    resize( QSize(511, 282).expandedTo(minimumSizeHint()) );
    clearWState( WState_Polished );

    // signals and slots connections
    connect( buttonOk, SIGNAL( clicked() ), this, SLOT( accept() ) );
    connect( buttonCancel, SIGNAL( clicked() ), this, SLOT( reject() ) );
}

/*
 *  Destroys the object and frees any allocated resources
 */
PhoneBook::~PhoneBook()
{
    // no need to delete child widgets, Qt does it all for us
}

/*
 *  Sets the strings of the subwidgets using the current
 *  language.
 */
void PhoneBook::languageChange()
{
    setCaption( tr( "Phone book" ) );
    buttonHelp->setText( tr( "&Help" ) );
    buttonHelp->setAccel( QKeySequence( tr( "F1" ) ) );
    buttonOk->setText( tr( "&OK" ) );
    buttonOk->setAccel( QKeySequence( QString::null ) );
    buttonCancel->setText( tr( "&Cancel" ) );
    buttonCancel->setAccel( QKeySequence( QString::null ) );
    ContactList->header()->setLabel( 0, tr( "Name" ) );
    ContactList->header()->setLabel( 1, tr( "Address SIP" ) );
    ContactList->header()->setLabel( 2, tr( "Mail" ) );
    ContactList->header()->setLabel( 3, tr( "Phone" ) );
    buttonOk_2->setText( tr( "&Add" ) );
    buttonOk_2->setAccel( QKeySequence( tr( "Alt+A" ) ) );
    buttonOk_3->setText( tr( "&Delete" ) );
    buttonOk_3->setAccel( QKeySequence( tr( "Alt+D" ) ) );
    buttonOk_4->setText( tr( "&Modify" ) );
    buttonOk_4->setAccel( QKeySequence( tr( "Alt+M" ) ) );
}

