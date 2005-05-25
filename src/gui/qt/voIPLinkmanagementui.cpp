/****************************************************************************
** Form implementation generated from reading ui file 'gui/qt/voIPLinkmanagement.ui'
**
** Created: Wed May 25 16:13:46 2005
**      by: The User Interface Compiler ($Id$)
**
** WARNING! All changes made in this file will be lost!
****************************************************************************/

#include "gui/qt/voIPLinkmanagementui.h"

#include <qvariant.h>
#include <qpushbutton.h>
#include <qlistbox.h>
#include <qgroupbox.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qcheckbox.h>
#include <qlayout.h>
#include <qtooltip.h>
#include <qwhatsthis.h>
#include <qimage.h>
#include <qpixmap.h>

#include "voIPLinkmanagement.ui.h"
static const unsigned char image0_data[] = { 
    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d,
    0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x0d, 0x00, 0x00, 0x00, 0x10,
    0x08, 0x06, 0x00, 0x00, 0x00, 0xcd, 0xa3, 0xf5, 0x39, 0x00, 0x00, 0x00,
    0x95, 0x49, 0x44, 0x41, 0x54, 0x28, 0x91, 0xdd, 0x91, 0x31, 0x0e, 0xc3,
    0x20, 0x0c, 0x45, 0x1f, 0x4c, 0x8c, 0x9c, 0x23, 0xb9, 0x01, 0x39, 0xbf,
    0x19, 0xb3, 0x71, 0x0f, 0x46, 0x36, 0x77, 0x29, 0x11, 0xb8, 0x56, 0xd5,
    0xb9, 0x5f, 0x42, 0x08, 0x9e, 0xf0, 0xc3, 0x10, 0xda, 0xdd, 0x60, 0x00,
    0x09, 0xfa, 0xe8, 0xe4, 0x94, 0x61, 0xc0, 0x51, 0x4e, 0xad, 0x55, 0xc2,
    0x5c, 0xaf, 0x3c, 0xf6, 0xd1, 0xb7, 0x8d, 0x3e, 0x3a, 0x47, 0x39, 0x15,
    0xa0, 0x94, 0x4b, 0x3d, 0x1e, 0xda, 0xdd, 0x3e, 0x0c, 0x98, 0x58, 0x63,
    0xf4, 0x0c, 0x36, 0xd6, 0x18, 0x67, 0x85, 0x52, 0x2e, 0xf7, 0x80, 0x3d,
    0x98, 0x53, 0x06, 0xa9, 0x82, 0x82, 0xfe, 0x3a, 0xa4, 0x0a, 0x41, 0xe1,
    0xab, 0xc1, 0x4b, 0x90, 0x2a, 0x4f, 0x4f, 0x73, 0x5e, 0xaf, 0x3a, 0x1f,
    0x61, 0xe5, 0x4f, 0x4f, 0x2b, 0x58, 0xe3, 0x71, 0xf7, 0x9f, 0xd6, 0x78,
    0x3c, 0x6e, 0x86, 0x77, 0x45, 0x6b, 0xb2, 0x7c, 0xfb, 0xa7, 0x59, 0xd1,
    0x9a, 0x2c, 0xff, 0xc7, 0x9e, 0x5e, 0x40, 0x48, 0xdc, 0xd7, 0x0e, 0xb2,
    0xad, 0xc5, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42,
    0x60, 0x82
};

static const unsigned char image1_data[] = { 
    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d,
    0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x0d, 0x00, 0x00, 0x00, 0x10,
    0x08, 0x06, 0x00, 0x00, 0x00, 0xcd, 0xa3, 0xf5, 0x39, 0x00, 0x00, 0x00,
    0x89, 0x49, 0x44, 0x41, 0x54, 0x28, 0x91, 0xdd, 0x91, 0x31, 0x12, 0xc3,
    0x30, 0x08, 0x04, 0x0f, 0x2a, 0x95, 0x7a, 0x87, 0xd3, 0xe5, 0xff, 0x6f,
    0xa0, 0x43, 0xef, 0xa0, 0xa4, 0x53, 0x8a, 0x58, 0x19, 0x44, 0x94, 0xd8,
    0x6e, 0x7d, 0x8d, 0x34, 0xdc, 0x2c, 0x37, 0xcc, 0x91, 0x8a, 0x02, 0x0e,
    0xa0, 0x00, 0xe6, 0x86, 0x5a, 0x2a, 0xb6, 0xe7, 0xa3, 0x63, 0x57, 0x13,
    0xa5, 0xec, 0xb3, 0xb9, 0x4d, 0x03, 0x73, 0x43, 0xd4, 0xca, 0xe7, 0xf1,
    0xa9, 0xa5, 0x02, 0x8e, 0xf7, 0x1b, 0xb4, 0xf2, 0x79, 0x4a, 0xd8, 0x37,
    0xe6, 0xa4, 0xec, 0x73, 0xdc, 0xf0, 0xd9, 0x98, 0x92, 0xb2, 0x7f, 0xc7,
    0x9b, 0xa8, 0x03, 0x1d, 0x17, 0xc5, 0x4d, 0x94, 0xae, 0x00, 0x4d, 0x94,
    0xd8, 0xdc, 0x70, 0x16, 0x6c, 0xa2, 0x34, 0xdd, 0x74, 0x04, 0x0e, 0xe0,
    0xab, 0xa7, 0x5f, 0x60, 0x04, 0x96, 0x3d, 0x65, 0x30, 0x02, 0x7f, 0x7b,
    0x1a, 0x60, 0x4e, 0x18, 0xfe, 0x0b, 0x97, 0x85, 0xc5, 0x5d, 0xe7, 0xfa,
    0xf4, 0x53, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42,
    0x60, 0x82
};


/*
 *  Constructs a VoIPLinkManagement as a child of 'parent', with the
 *  name 'name' and widget flags set to 'f'.
 */
VoIPLinkManagement::VoIPLinkManagement( QWidget* parent, const char* name, WFlags fl )
    : QWidget( parent, name, fl )
{
    QImage img;
    img.loadFromData( image0_data, sizeof( image0_data ), "PNG" );
    image0 = img;
    img.loadFromData( image1_data, sizeof( image1_data ), "PNG" );
    image1 = img;
    if ( !name )
	setName( "VoIPLinkManagement" );

    QWidget* privateLayoutWidget = new QWidget( this, "layout15" );
    privateLayoutWidget->setGeometry( QRect( 11, 31, 563, 270 ) );
    layout15 = new QGridLayout( privateLayoutWidget, 1, 1, 11, 6, "layout15"); 

    layout30 = new QVBoxLayout( 0, 0, 6, "layout30"); 

    listVoiplink = new QListBox( privateLayoutWidget, "listVoiplink" );
    listVoiplink->setEnabled( TRUE );
    listVoiplink->setCursor( QCursor( 13 ) );
    listVoiplink->setSelectionMode( QListBox::Single );
    layout30->addWidget( listVoiplink );

    buttonAddVoiplink = new QPushButton( privateLayoutWidget, "buttonAddVoiplink" );
    layout30->addWidget( buttonAddVoiplink );

    buttonRemoveVoiplink = new QPushButton( privateLayoutWidget, "buttonRemoveVoiplink" );
    layout30->addWidget( buttonRemoveVoiplink );

    up = new QPushButton( privateLayoutWidget, "up" );
    up->setPixmap( image0 );
    layout30->addWidget( up );

    down = new QPushButton( privateLayoutWidget, "down" );
    down->setPixmap( image1 );
    layout30->addWidget( down );

    layout15->addLayout( layout30, 0, 0 );

    parameters = new QGroupBox( privateLayoutWidget, "parameters" );

    QWidget* privateLayoutWidget_2 = new QWidget( parameters, "layout14" );
    privateLayoutWidget_2->setGeometry( QRect( 12, 22, 390, 230 ) );
    layout14 = new QVBoxLayout( privateLayoutWidget_2, 11, 6, "layout14"); 

    layout26 = new QGridLayout( 0, 1, 1, 0, 6, "layout26"); 

    layout24 = new QVBoxLayout( 0, 0, 6, "layout24"); 

    textLabel1 = new QLabel( privateLayoutWidget_2, "textLabel1" );
    layout24->addWidget( textLabel1 );

    textLabel2 = new QLabel( privateLayoutWidget_2, "textLabel2" );
    layout24->addWidget( textLabel2 );

    textLabel3 = new QLabel( privateLayoutWidget_2, "textLabel3" );
    layout24->addWidget( textLabel3 );

    textLabel4 = new QLabel( privateLayoutWidget_2, "textLabel4" );
    layout24->addWidget( textLabel4 );

    textLabel5 = new QLabel( privateLayoutWidget_2, "textLabel5" );
    layout24->addWidget( textLabel5 );

    textLabel6 = new QLabel( privateLayoutWidget_2, "textLabel6" );
    layout24->addWidget( textLabel6 );

    layout26->addLayout( layout24, 0, 0 );

    layout23 = new QVBoxLayout( 0, 0, 6, "layout23"); 

    yourName = new QLineEdit( privateLayoutWidget_2, "yourName" );
    layout23->addWidget( yourName );

    userPart = new QLineEdit( privateLayoutWidget_2, "userPart" );
    layout23->addWidget( userPart );

    hostPart = new QLineEdit( privateLayoutWidget_2, "hostPart" );
    layout23->addWidget( hostPart );

    authUser = new QLineEdit( privateLayoutWidget_2, "authUser" );
    layout23->addWidget( authUser );

    password = new QLineEdit( privateLayoutWidget_2, "password" );
    layout23->addWidget( password );

    proxy = new QLineEdit( privateLayoutWidget_2, "proxy" );
    layout23->addWidget( proxy );

    layout26->addLayout( layout23, 0, 1 );
    layout14->addLayout( layout26 );

    autoRegister = new QCheckBox( privateLayoutWidget_2, "autoRegister" );
    layout14->addWidget( autoRegister );

    layout15->addWidget( parameters, 0, 1 );
    languageChange();
    resize( QSize(589, 323).expandedTo(minimumSizeHint()) );
    clearWState( WState_Polished );

    // signals and slots connections
    connect( listVoiplink, SIGNAL( clicked(QListBoxItem*) ), this, SLOT( changeParamSlot() ) );
    connect( up, SIGNAL( clicked() ), this, SLOT( moveUpItemSlot() ) );
    connect( down, SIGNAL( clicked() ), this, SLOT( moveDownItemSlot() ) );
    connect( buttonRemoveVoiplink, SIGNAL( clicked() ), this, SLOT( removeVoIPLinkSlot() ) );
    connect( buttonAddVoiplink, SIGNAL( clicked() ), this, SLOT( addVoIPLinkSlot() ) );

    // tab order
    setTabOrder( yourName, userPart );
    setTabOrder( userPart, hostPart );
    setTabOrder( hostPart, authUser );
    setTabOrder( authUser, password );
    setTabOrder( password, proxy );
    setTabOrder( proxy, autoRegister );
    setTabOrder( autoRegister, listVoiplink );
    setTabOrder( listVoiplink, buttonAddVoiplink );
    setTabOrder( buttonAddVoiplink, buttonRemoveVoiplink );
    setTabOrder( buttonRemoveVoiplink, up );
    setTabOrder( up, down );
    init();
}

/*
 *  Destroys the object and frees any allocated resources
 */
VoIPLinkManagement::~VoIPLinkManagement()
{
    // no need to delete child widgets, Qt does it all for us
}

/*
 *  Sets the strings of the subwidgets using the current
 *  language.
 */
void VoIPLinkManagement::languageChange()
{
    setCaption( tr( "VoIP Link management" ) );
    listVoiplink->clear();
    listVoiplink->insertItem( tr( "SIP sfl" ) );
    listVoiplink->insertItem( tr( "SIP fwd" ) );
    listVoiplink->insertItem( tr( "IAX sfl" ) );
    listVoiplink->setCurrentItem( 0 );
    buttonAddVoiplink->setText( tr( "Add VoIP Link" ) );
    buttonRemoveVoiplink->setText( tr( "Remove VoIP Link" ) );
    up->setText( QString::null );
    down->setText( QString::null );
    parameters->setTitle( QString::null );
    textLabel1->setText( tr( "Your name" ) );
    textLabel2->setText( tr( "User part of URL" ) );
    textLabel3->setText( tr( "Host part of URL" ) );
    textLabel4->setText( tr( "Authorization user" ) );
    textLabel5->setText( tr( "Password" ) );
    textLabel6->setText( tr( "Proxy" ) );
    autoRegister->setText( tr( "Auto-register" ) );
}

