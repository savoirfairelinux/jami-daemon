/****************************************************************************
** Form implementation generated from reading ui file 'gui/qt/voIPLinkmanagement.ui'
**
** Created: Thu May 26 16:51:18 2005
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
    0x95, 0x49, 0x44, 0x41, 0x54, 0x28, 0x91, 0xdd, 0x91, 0xb1, 0x0d, 0xc4,
    0x20, 0x0c, 0x45, 0x9f, 0xa9, 0xb3, 0x40, 0xca, 0x64, 0x0c, 0x32, 0xbf,
    0x19, 0x20, 0x03, 0xd0, 0xb2, 0x00, 0x3d, 0x69, 0xce, 0x11, 0xf1, 0xf9,
    0x74, 0x57, 0xdf, 0x97, 0x10, 0x82, 0x27, 0xfc, 0x30, 0x48, 0x3d, 0x2b,
    0x74, 0x60, 0x81, 0xd6, 0x1b, 0xeb, 0xb2, 0x42, 0x87, 0x2d, 0xef, 0xa3,
    0x14, 0x15, 0x5b, 0xcf, 0x3c, 0xb5, 0xde, 0x1e, 0x1b, 0xad, 0x37, 0xb6,
    0xbc, 0x0f, 0x80, 0x9c, 0x8f, 0x11, 0x71, 0xa9, 0x67, 0x7d, 0x33, 0xe0,
    0xe2, 0x8d, 0x29, 0x32, 0xf8, 0x78, 0xa3, 0x58, 0x4f, 0x9f, 0x0e, 0x84,
    0x46, 0x2d, 0xca, 0x80, 0xf1, 0xeb, 0xd0, 0xa2, 0xc8, 0x80, 0xaf, 0x06,
    0x1f, 0xd1, 0xa2, 0x77, 0x4f, 0x36, 0xe7, 0x7c, 0xdc, 0x85, 0xec, 0x4a,
    0x33, 0x4f, 0xf6, 0x2a, 0x33, 0x98, 0x13, 0xf1, 0xf0, 0x9f, 0xe6, 0x44,
    0x3c, 0x3d, 0x0c, 0xaf, 0x8a, 0xde, 0xe4, 0xf9, 0xe3, 0x9f, 0xac, 0xa2,
    0x37, 0x79, 0xfe, 0x8f, 0x3d, 0x5d, 0x96, 0x01, 0xdc, 0x62, 0xf8, 0x1b,
    0x17, 0x7a, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42,
    0x60, 0x82
};

static const unsigned char image1_data[] = { 
    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d,
    0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x0d, 0x00, 0x00, 0x00, 0x10,
    0x08, 0x06, 0x00, 0x00, 0x00, 0xcd, 0xa3, 0xf5, 0x39, 0x00, 0x00, 0x00,
    0x8a, 0x49, 0x44, 0x41, 0x54, 0x28, 0x91, 0xdd, 0x91, 0xb1, 0x0d, 0xc3,
    0x30, 0x0c, 0x04, 0x9f, 0xac, 0xb5, 0x80, 0x4a, 0x67, 0x80, 0xec, 0x3f,
    0x07, 0x07, 0xa0, 0x4a, 0x2e, 0xc0, 0x5e, 0x29, 0x62, 0x05, 0x14, 0xa3,
    0xc4, 0x76, 0xeb, 0x6f, 0x24, 0xf0, 0x71, 0x7c, 0x10, 0x4f, 0x2a, 0x0a,
    0x38, 0x80, 0x02, 0x98, 0x1b, 0x6a, 0xa9, 0xd8, 0x9e, 0x8f, 0x8e, 0x5d,
    0x4d, 0x94, 0xb2, 0xcf, 0xe6, 0x36, 0x0d, 0xcc, 0x0d, 0x51, 0x2b, 0x9f,
    0xc7, 0xa7, 0x96, 0x0a, 0x38, 0xde, 0x6f, 0xd0, 0xca, 0xe7, 0x29, 0x61,
    0xdf, 0x98, 0x93, 0xb2, 0xcf, 0x71, 0xc3, 0x67, 0x63, 0x4a, 0xca, 0xfe,
    0x1d, 0x6f, 0xa2, 0x0e, 0x74, 0x5c, 0x14, 0x37, 0x51, 0xba, 0x02, 0x34,
    0x51, 0x62, 0x73, 0xc3, 0x59, 0xb0, 0x89, 0xd2, 0x74, 0xd3, 0x11, 0x38,
    0x80, 0xaf, 0x9e, 0x7e, 0x81, 0x11, 0x58, 0xf6, 0x94, 0xc1, 0x08, 0xfc,
    0xed, 0x69, 0x80, 0x39, 0x61, 0xf8, 0x2f, 0x07, 0x46, 0xc5, 0xf7, 0xb2,
    0xf4, 0xf3, 0x2c, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae,
    0x42, 0x60, 0x82
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
    privateLayoutWidget->setGeometry( QRect( 11, 31, 563, 280 ) );
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

    fullName = new QLineEdit( privateLayoutWidget_2, "fullName" );
    layout23->addWidget( fullName );

    userPart = new QLineEdit( privateLayoutWidget_2, "userPart" );
    layout23->addWidget( userPart );

    hostPart = new QLineEdit( privateLayoutWidget_2, "hostPart" );
    layout23->addWidget( hostPart );

    authUser = new QLineEdit( privateLayoutWidget_2, "authUser" );
    layout23->addWidget( authUser );

    password = new QLineEdit( privateLayoutWidget_2, "password" );
    password->setEchoMode( QLineEdit::Password );
    layout23->addWidget( password );

    proxy = new QLineEdit( privateLayoutWidget_2, "proxy" );
    proxy->setEchoMode( QLineEdit::Normal );
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
    setTabOrder( fullName, userPart );
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

