/****************************************************************************
** Form implementation generated from reading ui file 'url_input.ui'
**
** Created: Thu Mar 31 12:04:22 2005
**      by: The User Interface Compiler ($Id$)
**
** WARNING! All changes made in this file will be lost!
****************************************************************************/

#include "url_inputui.h"

#include <qvariant.h>
#include <qpushbutton.h>
#include <qlineedit.h>
#include <qlayout.h>
#include <qtooltip.h>
#include <qwhatsthis.h>
#include "url_input.ui.h"

/*
 *  Constructs a URL_Input as a child of 'parent', with the
 *  name 'name' and widget flags set to 'f'.
 *
 *  The dialog will by default be modeless, unless you set 'modal' to
 *  TRUE to construct a modal dialog.
 */
URL_Input::URL_Input( QWidget* parent, const char* name, bool modal, WFlags fl )
    : QDialog( parent, name, modal, fl )
{
    if ( !name )
	setName( "URL_Input" );

    buttonCancel = new QPushButton( this, "buttonCancel" );
    buttonCancel->setGeometry( QRect( 228, -1, 20, 23 ) );
    buttonCancel->setMinimumSize( QSize( 0, 23 ) );
    buttonCancel->setMaximumSize( QSize( 20, 23 ) );

    url = new QLineEdit( this, "url" );
    url->setGeometry( QRect( -1, 0, 210, 23 ) );
    url->setMinimumSize( QSize( 210, 0 ) );

    buttonOK = new QPushButton( this, "buttonOK" );
    buttonOK->setGeometry( QRect( 208, -1, 20, 23 ) );
    buttonOK->setMinimumSize( QSize( 0, 23 ) );
    buttonOK->setMaximumSize( QSize( 20, 23 ) );
    languageChange();
    resize( QSize(250, 21).expandedTo(minimumSizeHint()) );
    clearWState( WState_Polished );

    // signals and slots connections
    connect( buttonCancel, SIGNAL( clicked() ), this, SLOT( reject() ) );

    // tab order
    setTabOrder( url, buttonOK );
}

/*
 *  Destroys the object and frees any allocated resources
 */
URL_Input::~URL_Input()
{
    // no need to delete child widgets, Qt does it all for us
}

/*
 *  Sets the strings of the subwidgets using the current
 *  language.
 */
void URL_Input::languageChange()
{
    setCaption( tr( "URL_Input" ) );
    buttonCancel->setText( tr( "0" ) );
    buttonCancel->setAccel( QKeySequence( QString::null ) );
    buttonOK->setText( tr( "1" ) );
    buttonOK->setAccel( QKeySequence( QString::null ) );
}

